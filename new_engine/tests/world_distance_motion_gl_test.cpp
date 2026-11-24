/* Controlled actual-asset perspective sequences, not an FPS benchmark.
 * Zero camera/wind, fixed lighting; compare 8-phase jitter to held projection,
 * individual LODs to complementary overlap, and textured to flat RGB controls.
 * Build with world_models.cpp/world_temporal.cpp/texture_cache.c. */
#define GL_GLEXT_PROTOTYPES
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <SDL2/SDL.h>
#include "gl_helpers.h"
#include "engine/world_models.h"
#include "engine/world_temporal.h"
extern "C" {
#include "engine/texture_cache.h"
}
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

namespace {
constexpr int width=384,height=216,frames=48,first=32;
using Pixel=std::array<float,4>;
std::string source(const char *path){std::ifstream f(path);assert(f);std::ostringstream s;s<<f.rdbuf();return s.str();}
void f(GLuint p,const char *n,float x){glUniform1f(glGetUniformLocation(p,n),x);}
void i(GLuint p,const char *n,int x){glUniform1i(glGetUniformLocation(p,n),x);}
void v(GLuint p,const char *n,float x,float y,float z){glUniform3f(glGetUniformLocation(p,n),x,y,z);}
struct Input {
    GLuint fbo=0,color=0,depth=0;
    Input(){
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,width,height,0,GL_RGBA,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
        glGenTextures(1,&depth);glBindTexture(GL_TEXTURE_2D,depth);
        glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,width,height,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth,0);
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    }
    ~Input(){glDeleteTextures(1,&color);glDeleteTextures(1,&depth);glDeleteFramebuffers(1,&fbo);}
};
std::vector<Pixel> color(GLuint texture){
    std::vector<Pixel> out(width*height);glBindTexture(GL_TEXTURE_2D,texture);
    glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,out.data());return out;
}
std::vector<Pixel> canonical(const std::vector<Pixel> &in,const float jitter[2]){
    std::vector<Pixel> out(in.size());
    for(int y=0;y<height;y++)for(int x=0;x<width;x++) {
        float px=x+jitter[0],py=y+jitter[1];int ax=int(std::floor(px)),ay=int(std::floor(py));float tx=px-ax,ty=py-ay;
        Pixel p={};for(int j=0;j<2;j++)for(int k=0;k<2;k++) {
            const auto &q=in[std::clamp(ay+j,0,height-1)*width+std::clamp(ax+k,0,width-1)];
            float w=(j?ty:1-ty)*(k?tx:1-tx);for(int c=0;c<4;c++)p[c]+=q[c]*w;
        }out[y*width+x]=p;
    }return out;
}
void projection(WorldTemporalFrame &frame){
    auto *p=frame.view_projection;std::fill(p,p+16,0.f);p[5]=1/std::tan(.54f);p[0]=p[5]*height/width;
    p[8]=-2*frame.jitter_pixels[0]/width;p[9]=-2*frame.jitter_pixels[1]/height;
    p[10]=(15000.f+.4f)/(.4f-15000.f);p[11]=-1;p[14]=2*.4f*15000.f/(.4f-15000.f);
}
struct Metrics {double raw=0,canonical=0,resolved=0,history=0,interior_resolved=0,edge_resolved=0;std::size_t pixels=0,interior_pixels=0;};
struct Sequence {std::vector<std::vector<Pixel>> raw,canonical,resolved;std::vector<bool> used=std::vector<bool>(width*height);std::vector<unsigned> hits=std::vector<unsigned>(width*height);};
double variance(const std::vector<std::vector<Pixel>> &frames,const std::vector<bool> &used){
    double sum=0;std::size_t count=0;
    for(std::size_t p=0;p<used.size();p++)if(used[p])for(int c=0;c<3;c++) {
        double m=0,s=0;for(const auto &f:frames){m+=f[p][c];s+=double(f[p][c])*f[p][c];}
        sum+=std::max(0.,s/frames.size()-m*m/(frames.size()*frames.size()));count++;
    }assert(count);return sum/count;
}
void configure(GLuint p,const WorldTemporalFrame &frame,float eye){
    glUseProgram(p);glUniformMatrix4fv(glGetUniformLocation(p,"uVP"),1,GL_FALSE,frame.view_projection);
    v(p,"uOffset",0,-eye,0);glUniform2f(glGetUniformLocation(p,"uWorldAnchor"),0,0);
    i(p,"uGrass",0);f(p,"uWind",0);f(p,"uTime",22);f(p,"uCloud",0);f(p,"uWetness",0);f(p,"uHaze",0);
    i(p,"uWindHistoryEnabled",1);f(p,"uWindFrameDelta",0);f(p,"uActorReactive",0);
    v(p,"uSun",.4f,.8f,.4472136f);v(p,"uSunColor",1.2f,1.08f,.88f);
    v(p,"uSkyLight",.23f,.29f,.38f);v(p,"uGroundLight",.07f,.065f,.05f);v(p,"uAmbient",.2f,.2f,.2f);
    f(p,"uPixelScale",height*.5f/std::tan(.54f));i(p,"uShadow",7);
    i(p,"uLocalProbe0",4);i(p,"uLocalProbe1",5);i(p,"uLocalProbeCount",0);f(p,"uLocalIrradianceWeight",0);
    i(p,"uVillageSkyMask",12);i(p,"uVillageSkyDirectional",13);i(p,"uVillageSkyUp",14);i(p,"uVillageSkyEnabled",0);
    // Off-frustum shadow projection makes visibility1 without a shadow draw.
    float light[16]={1,0,0,0,0,1,0,0,0,0,1,0,10,10,10,1};
    glUniformMatrix4fv(glGetUniformLocation(p,"uLightVP"),1,GL_FALSE,light);
}
void check_geometry(GLuint program,const std::string &path,float distance,float eye) {
    cgltf_options options={};cgltf_data *data=nullptr;assert(cgltf_parse_file(&options,path.c_str(),&data)==cgltf_result_success);
    assert(cgltf_load_buffers(&options,data,path.c_str())==cgltf_result_success);
    std::vector<float> positions;float minimum_y=1e20f;
    for(std::size_t n=0;n<data->nodes_count;n++)if(data->nodes[n].mesh) {
        float matrix[16];cgltf_node_transform_world(data->nodes+n,matrix);
        for(std::size_t p=0;p<data->nodes[n].mesh->primitives_count;p++) {
            const auto &primitive=data->nodes[n].mesh->primitives[p];const cgltf_accessor *accessor=nullptr;
            for(std::size_t a=0;a<primitive.attributes_count;a++)if(primitive.attributes[a].type==cgltf_attribute_type_position)accessor=primitive.attributes[a].data;
            assert(accessor);for(std::size_t k=0;k<accessor->count;k++) {
                float in[3],out[3];assert(cgltf_accessor_read_float(accessor,k,in,3));
                for(int c=0;c<3;c++)out[c]=matrix[c]*in[0]+matrix[c+4]*in[1]+matrix[c+8]*in[2]+matrix[c+12];
                minimum_y=std::min(minimum_y,out[1]);positions.insert(positions.end(),out,out+3);
            }
        }
    }
    cgltf_free(data);for(std::size_t k=1;k<positions.size();k+=3)positions[k]-=minimum_y;
    GLuint buffer=0,vertices=0,vao=0,query=0;glGenBuffers(1,&buffer);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,8*1024*1024,nullptr,GL_STREAM_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);
    glGenBuffers(1,&vertices);glBindBuffer(GL_ARRAY_BUFFER,vertices);glBufferData(GL_ARRAY_BUFFER,positions.size()*sizeof(float),positions.data(),GL_STATIC_DRAW);
    glGenVertexArrays(1,&vao);glBindVertexArray(vao);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,nullptr);
    glVertexAttrib3f(1,0,1,0);glVertexAttrib2f(2,0,0);glVertexAttrib4f(3,1,0,0,1);glVertexAttrib4f(4,0,0,-distance,1);glVertexAttrib1f(5,0);
    glGenQueries(1,&query);std::vector<float> previous;
    for(int n=0;n<8;n++) {
        WorldTemporalFrame frame;frame.width=width;frame.height=height;world_temporal_jitter(n,frame.jitter_pixels);projection(frame);configure(program,frame,eye);
        i(program,"uVegetation",0);glEnable(GL_RASTERIZER_DISCARD);glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN,query);
        glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,GLsizei(positions.size()/3));glEndTransformFeedback();glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);glDisable(GL_RASTERIZER_DISCARD);
        GLuint count=0;glGetQueryObjectuiv(query,GL_QUERY_RESULT,&count);assert(count>0&&count*3*sizeof(float)<=8*1024*1024);
        std::vector<float> values(count*3);glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,values.size()*sizeof(float),values.data());
        if(n)assert(values==previous);else previous=values;
    }
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0);glBindVertexArray(0);glDeleteQueries(1,&query);glDeleteBuffers(1,&buffer);glDeleteBuffers(1,&vertices);glDeleteVertexArrays(1,&vao);
    assert(glGetError()==GL_NO_ERROR);std::printf("Transform feedback: %zu actual vertex positions byte-identical across8projection jitter phases\n",previous.size()/3);
}
Metrics run(GLuint program,WorldTemporal *taa,WorldModelAsset *a,WorldModelAsset *b,
            float distance,bool tree,int selected,bool jitter) {
    Input input;Sequence s;world_temporal_reset(taa);
    WorldTemporalFrame frame;frame.color=input.color;frame.depth=input.depth;frame.width=width;frame.height=height;frame.reactive_alpha=true;
    float eye=tree?12.f:7.f;frame.camera[1]=eye;
    for(int n=0;n<frames;n++) {
        if(jitter)world_temporal_jitter(n,frame.jitter_pixels);else frame.jitter_pixels[0]=frame.jitter_pixels[1]=0;
        projection(frame);glBindFramebuffer(GL_FRAMEBUFFER,input.fbo);glViewport(0,0,width,height);
        glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);
        glClearColor(.15f,.18f,.22f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        configure(program,frame,eye);
        for(int lod=0;lod<2;lod++)if(selected<0||selected==lod) {
            const WorldModelInstance root={0,tree&&lod==1?-.75f:0,-distance,0,1};
            auto *asset=lod?b:a;assert(world_model_upload_instances(asset,&root,1));
            if(tree){i(program,"uVegetation",lod?4:1);i(program,"uLodLevel",selected<0?lod+1:-1);f(program,"uTreeHeight",25);v(program,"uPropLodRange",0,0,0);}
            else{i(program,"uVegetation",0);i(program,"uLodLevel",-1);v(program,"uPropLodRange",selected<0?90.f:0.f,4,lod+1.f);}
            world_model_draw(asset,program);
        }
        if(n>=first) {
            std::vector<float> depth(width*height);glBindTexture(GL_TEXTURE_2D,input.depth);glGetTexImage(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,GL_FLOAT,depth.data());
            for(std::size_t p=0;p<depth.size();p++){s.used[p]=s.used[p]||depth[p]<1;s.hits[p]+=depth[p]<1;}
            s.raw.push_back(color(input.color));s.canonical.push_back(canonical(s.raw.back(),frame.jitter_pixels));
        }
        GLuint resolved=world_temporal_resolve(taa,frame);if(n>=first)s.resolved.push_back(color(resolved));
        assert(glGetError()==GL_NO_ERROR);
    }
    Metrics m;m.raw=variance(s.raw,s.used);m.canonical=variance(s.canonical,s.used);m.resolved=variance(s.resolved,s.used);
    std::vector<bool> interior(s.used.size()),edge(s.used.size());
    for(std::size_t p=0;p<s.used.size();p++){interior[p]=s.hits[p]==frames-first;edge[p]=s.used[p]&&!interior[p];m.interior_pixels+=interior[p];}
    if(m.interior_pixels)m.interior_resolved=variance(s.resolved,interior);
    if(m.interior_pixels<std::size_t(std::count(s.used.begin(),s.used.end(),true)))m.edge_resolved=variance(s.resolved,edge);
    for(std::size_t p=0;p<s.used.size();p++)if(s.used[p]){m.pixels++;for(const auto &f:s.resolved)m.history+=f[p][3]>1.5f?1:0;}
    m.history/=m.pixels*s.resolved.size();if(!jitter)assert(m.raw<1e-10&&m.canonical<1e-10&&m.resolved<1e-8);
    return m;
}
}
int main(int argc,char **argv){
    if(argc>1)assert(chdir(argv[1])==0);
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Controlled distance motion",0,0,width,height,SDL_WINDOW_HIDDEN|SDL_WINDOW_OPENGL);assert(window);
    auto context=SDL_GL_CreateContext(window);assert(context);std::printf("Distance sequence renderer: %s\n",glGetString(GL_RENDERER));
    std::string vs=source("shaders/world_foliage.vs.glsl"),fs=source("shaders/world_foliage.fs.glsl"),flat=fs;
    const std::string marker="    // A bounded derivative width antialiases the mask edge.";
    auto at=flat.find(marker);assert(at!=std::string::npos);flat.insert(at,"    base.rgb=vec3(.4,.35,.25);\n");
    GLuint textured=GLH_program(vs.c_str(),fs.c_str()),uniform=GLH_program(vs.c_str(),flat.c_str());assert(textured&&uniform);
    const char *varyings[]={"vRelative"};glTransformFeedbackVaryings(textured,1,varyings,GL_INTERLEAVED_ATTRIBS);glLinkProgram(textured);
    GLint linked=0;glGetProgramiv(textured,GL_LINK_STATUS,&linked);assert(linked);
    auto *taa=world_temporal_create("shaders/world_temporal.vs.glsl","shaders/world_temporal.fs.glsl");assert(taa);
    std::ostringstream json;json<<"{\n  \"renderer\":\""<<glGetString(GL_RENDERER)<<"\",\n  \"resolution\":["<<width<<','<<height<<"],\n  \"samples\":"<<frames-first<<",\n  \"controlled_camera_wind_lighting\":true,\n  \"cases\":[\n";bool comma=false;
    const std::string models=argc>3?argv[3]:"projects/valwood_world/models";
    for(bool tree:{false,true}) {
        const std::string stem=tree?"cinematic_pine":"purchased_village_house_a";
        auto *a=world_model_load((models+'/'+stem+(tree?"_mid.glb":"_near.glb")).c_str());
        auto *b=world_model_load((models+'/'+stem+(tree?"_far.glb":"_mid.glb")).c_str());assert(a&&b);
        check_geometry(textured,models+'/'+stem+(tree?"_mid.glb":"_near.glb"),tree?240.f:90.f,tree?12.f:7.f);
        for(int control=0;control<5;control++) {
            int selected=control<2?control:-1;bool jitter=control!=3;bool flat_rgb=control==4;
            const auto m=run(flat_rgb?uniform:textured,taa,a,b,tree?240.f:90.f,tree,selected,jitter);
            // An eight-phase overlap must converge instead of retaining its
            // canonical-current screen-door crawl. The old generic clipping
            // policy resolves ~15% of that variance; supported coverage <13%.
            if(tree&&control==2)assert(m.resolved<m.canonical*.13);
            if(!tree&&control==2)assert(m.resolved<m.canonical*.15);
            if(comma)json<<",\n";
            comma=true;
            const char *label=control==0?"first_lod":control==1?"second_lod":control==2?"overlap":control==3?"overlap_held_projection":"overlap_flat_rgb";
            json<<"    {\"asset\":\""<<stem<<"\",\"control\":\""<<label<<"\",\"pixels\":"<<m.pixels<<",\"raw_variance\":"<<m.raw<<",\"canonical_current_variance\":"<<m.canonical<<",\"resolved_variance\":"<<m.resolved<<",\"history_fraction\":"<<m.history<<",\"interior_pixels\":"<<m.interior_pixels<<",\"interior_resolved_variance\":"<<m.interior_resolved<<",\"edge_resolved_variance\":"<<m.edge_resolved<<'}';
            std::printf("%s %s pixels%zu raw%.9f canonical%.9f resolved%.9f history%.2f%% interior%zu variance%.9f edge%.9f\n",stem.c_str(),label,m.pixels,m.raw,m.canonical,m.resolved,m.history*100,m.interior_pixels,m.interior_resolved,m.edge_resolved);std::fflush(stdout);
        }
        world_model_free(a);world_model_free(b);
    }
    json<<"\n  ]\n}\n";if(argc>2){std::ofstream out(argv[2]);assert(out);out<<json.str();}
    world_temporal_free(taa);glDeleteProgram(textured);glDeleteProgram(uniform);texture_cache_shutdown();assert(glGetError()==GL_NO_ERROR);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}

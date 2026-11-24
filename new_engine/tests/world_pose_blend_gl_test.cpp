/* Exact GPU pose geometry/ownership regression; not a frame-rate benchmark.
 * SDL offscreen by default; VALWOOD_TEST_NATIVE_GL=1 retains the native driver.
 */
#define GL_GLEXT_PROTOTYPES
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <SDL2/SDL.h>
#include "gl_helpers.h"
#include "engine/world_models.h"
extern "C" {
#include "engine/texture_cache.h"
}
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

// GL3.3 forbids program changes during transform feedback. Suppress only the
// renderer's redundant same-program bind, asserting its program selection.
static bool capturing=false;
static GLuint capture_program=0;
extern "C" void __real_glUseProgram(GLuint);
extern "C" void __wrap_glUseProgram(GLuint p){if(capturing){assert(p==capture_program);return;}__real_glUseProgram(p);}

namespace {
using Sample=std::array<float,12>; // position, normal, tangent, bitangent
std::string text(const char *path){std::ifstream f(path);assert(f);std::ostringstream s;s<<f.rdbuf();return s.str();}
void integer(GLuint p,const char *n,int v){glUniform1i(glGetUniformLocation(p,n),v);}
GLuint program(const char *fragment){
    const auto vs=text("shaders/world_foliage.vs.glsl"),fs=text(fragment);
    GLuint p=GLH_program(vs.c_str(),fs.c_str());assert(p);
    const char *outputs[]={"vPos","vNormal","vTangent","vBitangent"};
    glTransformFeedbackVaryings(p,4,outputs,GL_INTERLEAVED_ATTRIBS);glLinkProgram(p);
    GLint linked=0;glGetProgramiv(p,GL_LINK_STATUS,&linked);assert(linked);
    glUseProgram(p);integer(p,"uLocalProbe0",4);integer(p,"uLocalProbe1",5);integer(p,"uLocalProbeCount",0);glUniform1f(glGetUniformLocation(p,"uLocalIrradianceWeight"),0);integer(p,"uVillageSkyMask",12);integer(p,"uVillageSkyDirectional",13);integer(p,"uVillageSkyUp",14);integer(p,"uGrass",0);integer(p,"uVegetation",0);
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    glUniformMatrix4fv(glGetUniformLocation(p,"uVP"),1,GL_FALSE,identity);
    return p;
}
size_t indices(const char *path){
    cgltf_options options={};cgltf_data *data=nullptr;assert(cgltf_parse_file(&options,path,&data)==cgltf_result_success);
    size_t total=0;for(size_t n=0;n<data->nodes_count;n++)if(data->nodes[n].mesh)
        for(size_t m=0;m<data->nodes[n].mesh->primitives_count;m++)total+=data->nodes[n].mesh->primitives[m].indices->count;
    cgltf_free(data);return total;
}
template<class Draw> std::vector<Sample> capture(GLuint p,size_t vertices,Draw draw){
    std::vector<Sample> result(vertices);GLuint buffer;glGenBuffers(1,&buffer);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,vertices*sizeof(Sample),nullptr,GL_DYNAMIC_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);glUseProgram(p);
    glEnable(GL_RASTERIZER_DISCARD);capture_program=p;capturing=true;
    glBeginTransformFeedback(GL_TRIANGLES);draw();glEndTransformFeedback();capturing=false;glDisable(GL_RASTERIZER_DISCARD);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,vertices*sizeof(Sample),result.data());
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0);glDeleteBuffers(1,&buffer);assert(glGetError()==GL_NO_ERROR);
    for(const auto &sample:result)for(float value:sample)assert(std::isfinite(value));
    return result;
}
void normalized(float *v){float length=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);assert(length>1e-5);for(int k=0;k<3;k++)v[k]/=length;}
std::string fixture(const char *name,int variant=0,bool mirrored=false,bool authored=true){
    struct V {float p[3],n[3],uv[2],t[4];};
    V vertices[]={{{0,0,0},{0,0,1},{0,0},{1,0,0,1}},{{1,0,0},{0,0,1},{1,0},{1,0,0,1}},{{0,1,0},{0,0,1},{0,1},{1,0,0,1}},{{0,1,0},{0,0,1},{0,-2},{1,0,0,1}}};
    uint32_t order[6]={0,1,2,0,1,3};
    const size_t nv=variant>=7?4:3,ni=variant>=7?6:3;
    if(variant==1){vertices[1].p[0]=1.6f;vertices[2].p[0]=.2f;vertices[2].p[1]=1.4f;}
    if(variant==2)vertices[1].uv[0]=.8f;
    if(variant==3)std::swap(order[1],order[2]);
    if(variant==4)for(auto &v:vertices)v.t[3]=-1;
    if(variant==5)vertices[1].p[0]=-1; // geometry fold against authored normals
    if(variant==6)for(auto &v:vertices)v.uv[0]=v.uv[1]=0;
    if(variant==8)vertices[1].p[0]=-1;
    if(variant==9)vertices[3].uv[1]=-1.9f;
    const size_t vertex_bytes=nv*sizeof(V),bytes=vertex_bytes+ni*sizeof(uint32_t);
    std::ostringstream json;
    json<<"{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"byteLength\":"<<bytes<<"}],\"bufferViews\":[{\"buffer\":0,\"byteLength\":"<<vertex_bytes<<",\"byteStride\":48},{\"buffer\":0,\"byteOffset\":"<<vertex_bytes<<",\"byteLength\":"<<ni*4<<"}],\"accessors\":[";
    for(int k=0;k<4;k++){if(k)json<<',';json<<"{\"bufferView\":0,\"byteOffset\":"<<std::array<int,4>{0,12,24,32}[k]<<",\"componentType\":5126,\"count\":"<<nv<<",\"type\":\""<<std::array<const char*,4>{"VEC3","VEC3","VEC2","VEC4"}[k]<<"\"}";}
    json<<",{\"bufferView\":1,\"componentType\":5125,\"count\":"<<ni<<",\"type\":\"SCALAR\"}],\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.4,0.4,0.4,1],\"metallicFactor\":0,\"roughnessFactor\":0.8}}],\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1,\"TEXCOORD_0\":2";
    if(authored)json<<",\"TANGENT\":3";
    json<<"},\"indices\":4,\"material\":0}]}],\"nodes\":[{\"mesh\":0";
    if(mirrored)json<<",\"scale\":[-1,1,1]";
    json<<"}],\"scenes\":[{\"nodes\":[0]}],\"scene\":0}";
    std::string encoded=json.str();while(encoded.size()%4)encoded+=' ';
    std::string path="/tmp/valwood-pose-"+std::string(name)+".glb";std::ofstream file(path,std::ios::binary);assert(file);
    auto word=[&](uint32_t value){file.write(reinterpret_cast<const char*>(&value),4);};
    word(0x46546c67);word(2);word(uint32_t(12+8+encoded.size()+8+bytes));word(uint32_t(encoded.size()));word(0x4e4f534a);file.write(encoded.data(),encoded.size());word(uint32_t(bytes));word(0x004e4942);file.write(reinterpret_cast<const char*>(vertices),vertex_bytes);file.write(reinterpret_cast<const char*>(order),ni*4);
    return path;
}
void check_pair(const char *path_a,const char *path_b,const std::array<GLuint,3>&programs,bool preserve_origin=false){
    auto load=preserve_origin?world_model_load_pose_origin:world_model_load_pose;
    auto *a=load(path_a,1),*b=load(path_b,1);assert(a&&b);
    const size_t count=indices(path_a);assert(count==indices(path_b));
    WorldModelInstance instance={.2f,.4f,-.1f,0,1};
    assert(world_model_upload_instances(a,&instance,1)&&world_model_upload_instances(b,&instance,1));
    const auto textures=texture_cache_stats();auto *batch=world_model_pose_blend_create(1,1);auto *other=world_model_pose_blend_create(1,1);assert(batch&&other);
    assert(world_model_pose_blend_set_targets(batch,a,b));assert(world_model_pose_blend_set_targets(other,b,a));
    const auto first=capture(programs[0],count,[&](){world_model_draw(a,programs[0]);});
    const auto last=capture(programs[0],count,[&](){world_model_draw(b,programs[0]);});
    double displacement=0;
    for(size_t k=0;k<count;k++)for(int c=0;c<3;c++)displacement+=std::fabs(first[k][c]-last[k][c]);
    assert(displacement>.01);
    for(float weight:{0.f,.25f,.5f,.75f,1.f}){
        assert(world_model_pose_blend_upload(batch,&instance,&weight,1));
        const auto blended=capture(programs[0],count,[&](){world_model_pose_blend_draw(batch,programs[0]);});
        if(weight==0)assert(blended==first);
        if(weight==1)assert(blended==last);
        for(size_t k=0;k<count;k++){
            for(int c=0;c<3;c++)assert(std::fabs(blended[k][c]-(first[k][c]*(1-weight)+last[k][c]*weight))<2e-5f);
            for(int start:{3,6,9}){
                float length=0;for(int c=0;c<3;c++)length+=blended[k][start+c]*blended[k][start+c];assert(std::fabs(length-1)<3e-5f);
            }
            float dot=0;for(int c=0;c<3;c++)dot+=blended[k][3+c]*blended[k][6+c];assert(std::fabs(dot)<3e-5f);
            if(weight>0&&weight<1){
                float expected_n[3],expected_t[3];for(int c=0;c<3;c++){expected_n[c]=first[k][3+c]*(1-weight)+last[k][3+c]*weight;expected_t[c]=first[k][6+c]*(1-weight)+last[k][6+c]*weight;}
                normalized(expected_n);float projected=0;for(int c=0;c<3;c++)projected+=expected_n[c]*expected_t[c];
                for(int c=0;c<3;c++)expected_t[c]-=expected_n[c]*projected;
                float tangent_length=0;for(float value:expected_t)tangent_length+=value*value;
                if(tangent_length>1e-8)normalized(expected_t);
                for(int c=0;c<3;c++){assert(std::fabs(blended[k][3+c]-expected_n[c])<3e-5f);if(tangent_length>1e-8)assert(std::fabs(blended[k][6+c]-expected_t[c])<3e-5f);}
            }
        }
        for(GLuint p:programs){
            const auto pass=capture(p,count,[&](){world_model_pose_blend_draw(batch,p);});assert(pass==blended);
            GLint enabled=1;glGetUniformiv(p,glGetUniformLocation(p,"uPoseBlendEnabled"),&enabled);assert(!enabled);
            const GLint fade_location=glGetUniformLocation(p,"uInstanceFade");
            if(fade_location>=0){GLint fade=1;glGetUniformiv(p,fade_location,&fade);assert(!fade);}
            else assert(p==programs[2]); // Shadow drops unused placement opacity.
        }
        const auto scoped_shadow=capture(programs[2],count,[&](){
            WorldModelDepthMaterialScope depth_materials(programs[2]);
            world_model_pose_blend_draw(batch,programs[2]);
        });
        assert(scoped_shadow==blended&&world_model_depth_material_program(0)==0);
        // Independent reversed target batches never overwrite this snapshot.
        float opposite=1-weight;assert(world_model_pose_blend_upload(other,&instance,&opposite,1));
        const auto reverse=capture(programs[0],count,[&](){world_model_pose_blend_draw(other,programs[0]);});
        for(size_t k=0;k<count;k++)for(int c=0;c<12;c++)assert(std::fabs(reverse[k][c]-blended[k][c])<3e-5f);
        assert(capture(programs[0],count,[&](){world_model_pose_blend_draw(batch,programs[0]);})==blended);
        for(float invalid:{-.01f,1.01f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
            assert(!world_model_pose_blend_upload(batch,&instance,&invalid,1));
            assert(capture(programs[0],count,[&](){world_model_pose_blend_draw(batch,programs[0]);})==blended);
        }
        assert(capture(programs[0],count,[&](){world_model_draw(a,programs[0]);})==first);
    }
    const auto after=texture_cache_stats();assert(textures.entries==after.entries&&textures.references==after.references&&textures.source_bytes==after.source_bytes);
    world_model_pose_blend_free(other);world_model_pose_blend_free(batch);world_model_free(b);world_model_free(a);
    std::printf("PASS GPU pose pair: %s, %zu indexed vertices; exact endpoints, continuous positions, orthonormal basis, identical color/reflection/shadow geometry, batch isolation, invalid-weight transaction, ordinary reset, unchanged images\n",path_a,count);
}
void check_link_rejection(GLuint program){
    const auto path=fixture("link-base"),next=fixture("link-next",1);
    auto *a=world_model_load_pose(path.c_str(),1),*b=world_model_load_pose(next.c_str(),1);
    auto *ordinary=world_model_load(path.c_str());assert(a&&b&&ordinary);
    auto *batch=world_model_pose_blend_create(1,1);assert(batch&&world_model_pose_blend_set_targets(batch,a,b));
    WorldModelInstance placement={0,0,0,0,1};float weight=.5f;assert(world_model_pose_blend_upload(batch,&placement,&weight,1));
    auto previous=capture(program,3,[&](){world_model_pose_blend_draw(batch,program);});
    for(int variant:{2,3,4}){
        const auto invalid_path=fixture("incompatible",variant);auto *bad=world_model_load_pose(invalid_path.c_str(),1);assert(bad);
        GLuint sentinel_vao,sentinel_buffer;glGenVertexArrays(1,&sentinel_vao);glGenBuffers(1,&sentinel_buffer);
        glBindVertexArray(sentinel_vao);glBindBuffer(GL_ARRAY_BUFFER,sentinel_buffer);
        assert(!world_model_pose_blend_set_targets(batch,a,bad));
        GLint state=0;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&state);assert(GLuint(state)==sentinel_vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&state);assert(GLuint(state)==sentinel_buffer);
        glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);glDeleteVertexArrays(1,&sentinel_vao);glDeleteBuffers(1,&sentinel_buffer);
        assert(capture(program,3,[&](){world_model_pose_blend_draw(batch,program);})==previous);world_model_free(bad);
    }
    assert(!world_model_pose_blend_set_targets(batch,a,ordinary));assert(!world_model_pose_blend_set_targets(batch,nullptr,b));
    assert(!world_model_pose_blend_upload(batch,&placement,&weight,2));
    placement.scale=0;assert(!world_model_pose_blend_upload(batch,&placement,&weight,1));placement.scale=1;
    assert(capture(program,3,[&](){world_model_pose_blend_draw(batch,program);})==previous);
    assert(world_model_pose_blend_upload(batch,nullptr,nullptr,0));
    world_model_pose_blend_free(batch);world_model_free(ordinary);world_model_free(b);world_model_free(a);
    // Generated-only parity is based on immutable UV topology, including a
    // mirrored node, degenerate UV fallback and a mixed-UV dominant face.
    for(bool mirror:{false,true})for(int variant:{0,6,7}){
        const auto one=fixture("generated-a",variant,mirror,false);
        const auto two=fixture("generated-b",variant==0?5:variant==7?8:6,mirror,false);
        a=world_model_load_pose(one.c_str(),1);b=world_model_load_pose(two.c_str(),1);assert(a&&b);
        batch=world_model_pose_blend_create(1,1);assert(batch&&world_model_pose_blend_set_targets(batch,a,b));
        assert(world_model_pose_blend_upload(batch,&placement,&weight,1));
        auto result=capture(program,variant==7?6:3,[&](){world_model_pose_blend_draw(batch,program);});
        for(const auto &v:result){float orientation=(v[4]*v[8]-v[5]*v[7])*v[9]+(v[5]*v[6]-v[3]*v[8])*v[10]+(v[3]*v[7]-v[4]*v[6])*v[11];assert(std::fabs(std::fabs(orientation)-1)<3e-5f);if(variant!=7)assert((orientation<0)==mirror);}
        if(variant==7){const auto bad_path=fixture("mixed-uv-change",9,mirror,false);auto *bad=world_model_load_pose(bad_path.c_str(),1);assert(bad&&!world_model_pose_blend_set_targets(batch,a,bad));world_model_free(bad);}
        world_model_pose_blend_free(batch);world_model_free(b);world_model_free(a);
    }
    std::puts("PASS pose link containment: UV/index/authored-sign rejection, exact invalid-state preservation, capacity/zero upload, fixed generated parity on folded/mirrored/degenerate/mixed-UV fixtures");
}
}
int main(int argc,char **argv){
    if(argc>1)assert(chdir(argv[1])==0);
    if(!std::getenv("VALWOOD_TEST_NATIVE_GL"))SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("GPU pose interpolation test",0,0,32,32,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);SDL_GLContext context=SDL_GL_CreateContext(window);assert(context);
    std::printf("GPU pose renderer: %s\n",glGetString(GL_RENDERER));
    const std::array<GLuint,3> programs={program("shaders/world_foliage.fs.glsl"),program("shaders/world_foliage.fs.glsl"),program("shaders/world_foliage_shadow.fs.glsl")};
    assert(!world_model_load_pose("irrelevant",0));assert(!world_model_pose_blend_create(0,1));assert(!world_model_pose_blend_create(4097,1));assert(!world_model_pose_blend_create(1,2));
    const auto first=fixture("authored-a"),last=fixture("authored-b",1);check_pair(first.c_str(),last.c_str(),programs);
    check_link_rejection(programs[0]);
    // Pose-origin loading must preserve the source's authored hand height.
    // Ordinary props retain their established base-aligned loader behavior.
    const char *arms="projects/valwood_world/models/combat_arms_000.glb";
    auto *origin=world_model_load_pose_origin(arms,1),*aligned=world_model_load_pose(arms,1);
    assert(origin&&aligned&&!world_model_load_pose_origin(arms,2));
    float olo[3],ohi[3],alo[3],ahi[3];assert(world_model_bounds(origin,olo,ohi)&&world_model_bounds(aligned,alo,ahi));
    assert(olo[1]>.5f&&std::abs(alo[1])<.00001f&&std::abs(ohi[1]-olo[1]-ahi[1])<.00001f);
    world_model_free(origin);world_model_free(aligned);
    check_pair("projects/valwood_world/models/combat_body_012.glb","projects/valwood_world/models/combat_body_013.glb",programs);

    check_pair("projects/valwood_world/models/purchased_character_wayfarer_walk_00.glb","projects/valwood_world/models/purchased_character_wayfarer_walk_01.glb",programs);
    check_pair("projects/valwood_world/models/longsword_mocap/combat_body_012.glb","projects/valwood_world/models/longsword_mocap/combat_body_013.glb",programs);
    check_pair("projects/valwood_world/models/longsword_mocap/combat_arms_012.glb","projects/valwood_world/models/longsword_mocap/combat_arms_013.glb",programs,true);
    check_pair("projects/valwood_world/models/castle_dragons/hellfire_flight_00.glb","projects/valwood_world/models/castle_dragons/hellfire_flight_breath_00.glb",programs,true);
    check_pair("projects/valwood_world/models/castle_dragons/frostshade_landing_04.glb","projects/valwood_world/models/castle_dragons/frostshade_landing_05.glb",programs,true);
    // Real five-primitive female: source cloth/skin, metal, hair, eyes and
    // lashes must interpolate the same indexed geometry in every render pass.
    // Cover adjacent standing keys, walking keys and the actual idle/walk
    // handover, including exact0/1 endpoints and fractional analytical TBN.
    check_pair("projects/valwood_world/models/village_character_travelerwoman_idle_00.glb","projects/valwood_world/models/village_character_travelerwoman_idle_01.glb",programs);
    check_pair("projects/valwood_world/models/village_character_travelerwoman_walk_00.glb","projects/valwood_world/models/village_character_travelerwoman_walk_01.glb",programs);
    check_pair("projects/valwood_world/models/village_character_travelerwoman_idle_00.glb","projects/valwood_world/models/village_character_travelerwoman_walk_00.glb",programs);
    for(GLuint p:programs)glDeleteProgram(p);
    assert(!texture_cache_stats().entries&&!texture_cache_stats().references);texture_cache_shutdown();
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();std::puts("world_pose_blend_gl_test: PASS");
}

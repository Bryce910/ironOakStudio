#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#undef CGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "engine/world_model_cpu.h"
#include "engine/world_model_image_bytes.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>
namespace fs=std::filesystem;
static unsigned checks=0;
static void require(bool v,const std::string &why){++checks;if(!v)throw std::runtime_error(why);}
static const std::vector<unsigned char> png={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,4,0,0,0,4,8,6,0,0,0,169,241,158,126,0,0,0,21,73,68,65,84,120,156,99,108,104,248,255,159,1,9,48,49,160,1,194,2,0,196,83,3,6,29,15,87,130,0,0,0,0,73,69,78,68,174,66,96,130};
static const std::string leaf(64,'a');
static void write(const fs::path &path,const std::vector<unsigned char>&bytes){fs::create_directories(path.parent_path());std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());require(bool(file),"fixture write");}
static void u32(std::vector<unsigned char>&out,unsigned value){for(unsigned i=0;i<4;++i)out.push_back(static_cast<unsigned char>(value>>(8*i)));}
static fs::path model(const fs::path &path,const std::string &uri={}){
    const float positions[]={0,0,0,1,0,0,0,1,0};std::vector<unsigned char> bin(sizeof(positions));std::memcpy(bin.data(),positions,sizeof(positions));
    if(uri.empty())bin.insert(bin.end(),png.begin(),png.end());while(bin.size()%4)bin.push_back(0);
    std::ostringstream json;json<<R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"material":0}]}],"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0},"metallicFactor":0,"roughnessFactor":0.8}}],"textures":[{"source":0}],"images":[{)";
    if(uri.empty())json<<R"("bufferView":1,"mimeType":"image/png")";else json<<R"("uri":")"<<uri<<'"';
    json<<R"(}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]}],"bufferViews":[{"buffer":0,"byteLength":36})";
    if(uri.empty())json<<R"(,{"buffer":0,"byteOffset":36,"byteLength":)"<<png.size()<<'}';
    json<<R"(],"buffers":[{"byteLength":)"<<bin.size()<<"}]}";auto text=json.str();while(text.size()%4)text.push_back(' ');
    std::vector<unsigned char> bytes;u32(bytes,0x46546c67);u32(bytes,2);u32(bytes,unsigned(28+text.size()+bin.size()));u32(bytes,unsigned(text.size()));u32(bytes,0x4e4f534a);bytes.insert(bytes.end(),text.begin(),text.end());u32(bytes,unsigned(bin.size()));u32(bytes,0x004e4942);bytes.insert(bytes.end(),bin.begin(),bin.end());write(path,bytes);return path;
}
static unsigned destroys=0;
static void destroy(unsigned){++destroys;}
int main(int argc,char **argv){
    const auto directory=fs::temp_directory_path()/("valwood-shared-images-"+std::to_string(getpid()));
    try{
        const auto bank=directory/"bank",image_path=bank/"textures"/"shared"/(leaf+".png");write(image_path,png);
        const auto embedded=model(bank/"models"/"embedded.glb"),external=model(bank/"models"/"external.glb","../textures/shared/"+leaf+".png"),nested=model(bank/"models"/"weapons"/"nested.glb","../../textures/shared/"+leaf+".png");
        namespace cpu=world_model_cpu;cpu::Options options;std::string error;
        const auto a=cpu::prepare(embedded.c_str(),options,error);require(bool(a),"embedded prepare: "+error);
        auto compare=[&](const fs::path &path){
            const auto b=cpu::prepare(path.c_str(),options,error);require(bool(b),"shared prepare: "+error);require(a->meshes.size()==b->meshes.size()&&a->textures.size()==b->textures.size(),"same mesh / texture counts");
            require(a->geometry_bytes==b->geometry_bytes&&a->texture_bytes==b->texture_bytes,"same GPU admission bytes");
            const auto &x=a->textures[0],&y=b->textures[0];require(x.encoded==y.encoded&&x.fingerprint==y.fingerprint&&x.srgb==y.srgb&&x.opaque==y.opaque,"exact encoded bytes and policy");
            require(x.levels.size()==y.levels.size(),"same mip chain");for(unsigned i=0;i<x.levels.size();++i)require(x.levels[i].width==y.levels[i].width&&x.levels[i].height==y.levels[i].height&&x.levels[i].rgba==y.levels[i].rgba,"identical decoded mip pixels");
            const TextureCachePolicy policy={true,0x2703,0x2601,0x2901,0x2901,1};
            auto *key=texture_cache_prepare(x.encoded.data(),x.encoded.size());require(key!=nullptr,"cache prepare");
            require(texture_cache_adopt_prepared(key,&policy,90210,destroy)==90210,"embedded cache owner");
            const auto shared=texture_cache_find_prepared(y.cache_key->prepared,&policy);require(shared==90210,"shared URI reuses exact embedded texture cache ID");
            require(texture_cache_release(shared)&&texture_cache_release(90210),"shared cache ownership teardown");require(texture_cache_stats().references==0&&texture_cache_stats().entries==0,"no cache references leaked");
        };
        const auto root_model=model(bank/"bow_body.glb","textures/shared/"+leaf+".png");
        compare(root_model);compare(external);compare(nested);
        fs::create_directories(directory/"project"/"models");const auto linked=directory/"project"/"models"/"linked.glb";fs::create_symlink(nested,linked);compare(linked);
        const auto linked_bow=directory/"project"/"models"/"linked_bow.glb";fs::create_symlink(root_model,linked_bow);compare(linked_bow);
        namespace images=world_model_image_bytes;images::Image bytes;cgltf_image image={};std::string uri="../textures/shared/"+leaf+".png";image.uri=uri.data();
        cgltf_buffer source_buffer={};source_buffer.data=const_cast<unsigned char*>(png.data());source_buffer.size=png.size();cgltf_buffer_view source_view={};source_view.buffer=&source_buffer;source_view.size=png.size();cgltf_image source_image={};source_image.buffer_view=&source_view;
        require(images::read(&source_image,embedded.c_str(),0,bytes,error)&&!bytes.external()&&bytes.length==png.size()&&!std::memcmp(bytes.data(),png.data(),png.size()),"embedded bytes use no local allocation or external budget");
        source_image.uri=uri.data();require(!images::read(&source_image,embedded.c_str(),images::maximum_bytes,bytes,error),"ambiguous embedded plus URI is rejected");
        require(images::read(&image,external.c_str(),png.size(),bytes,error)&&bytes.external()&&bytes.length==png.size()&&!std::memcmp(bytes.data(),png.data(),png.size()),"bounded exact local read");
        std::string bow_uri="textures/shared/"+leaf+".png";cgltf_image bow_image={};bow_image.uri=bow_uri.data();
        require(images::read(&bow_image,root_model.c_str(),png.size(),bytes,error)&&bytes.external()&&bytes.length==png.size()&&!std::memcmp(bytes.data(),png.data(),png.size()),"root-level bow shared image preserves exact bytes");
        require(!images::read(&bow_image,root_model.c_str(),png.size()-1,bytes,error)&&error.find("budget")!=std::string::npos,"root-level bow image respects encoded reservation budget");
        require(!images::read(&image,external.c_str(),png.size()-1,bytes,error)&&error.find("budget")!=std::string::npos&&bytes.data()==nullptr,"reject external encoded reservation over budget");
        const std::vector<std::string> bad={"/tmp/"+leaf+".png","https://example.org/"+leaf+".png","file:///tmp/"+leaf+".png","data:image/png;base64,aGVsbG8=","../../../textures/shared/"+leaf+".png","../textures/shared/../"+leaf+".png","../textures/shared/%2e%2e/"+leaf+".png","../textures/shared/"+leaf+".png?cache=1","../textures/shared/"+leaf+".png#x","../textures/shared/"+leaf+".jpeg","..\\textures\\shared\\"+leaf+".png","../textures/shared/AAAA.png","textures/shared/../"+leaf+".png","textures/shared/%2e%2e/"+leaf+".png","./textures/shared/"+leaf+".png","textures/shared/"+leaf+".png?x=1","../textures/shared/"+std::string(64,'g')+".png"};
        for(const auto &bad_uri:bad){image.uri=const_cast<char*>(bad_uri.c_str());require(!images::read(&image,external.c_str(),images::maximum_bytes,bytes,error)&&bytes.data()==nullptr,"reject unsupported / escaping URI: "+bad_uri);}
        uri="../textures/shared/"+std::string(64,'b')+".png";image.uri=uri.data();require(!images::read(&image,external.c_str(),images::maximum_bytes,bytes,error),"missing image fails");
        const auto missing_bow=model(bank/"bow_missing.glb","textures/shared/"+std::string(64,'b')+".png");require(!cpu::prepare(missing_bow.c_str(),options,error),"missing root-level bow image fails whole model");
        auto bad_model=model(bank/"models"/"missing.glb",uri);require(!cpu::prepare(bad_model.c_str(),options,error),"referenced missing image fails whole model instead of white fallback");
        uri="../textures/shared/"+leaf+".png";image.uri=uri.data();const auto outside=directory/"outside.png";write(outside,png);fs::remove(image_path);fs::create_symlink(outside,image_path);
        require(!images::read(&bow_image,root_model.c_str(),images::maximum_bytes,bytes,error),"root-level bow rejects symlink escape");
        require(!images::read(&image,external.c_str(),images::maximum_bytes,bytes,error),"reject symlink escape");require(!cpu::prepare(external.c_str(),options,error),"CPU loader rejects escaped shared image");fs::remove(image_path);write(image_path,png);
        fs::rename(bank/"textures"/"shared",bank/"textures"/"real");fs::create_directory_symlink(bank/"textures"/"real",bank/"textures"/"shared");require(!images::read(&image,external.c_str(),images::maximum_bytes,bytes,error),"reject redirected shared directory");fs::remove(bank/"textures"/"shared");fs::rename(bank/"textures"/"real",bank/"textures"/"shared");
        {std::ofstream huge(image_path,std::ios::binary|std::ios::trunc);huge.seekp(std::streamoff(images::maximum_bytes));huge.put('x');}require(!images::read(&image,external.c_str(),images::maximum_bytes,bytes,error)&&error.find("length")!=std::string::npos,"reject greater than64MiB before allocating");write(image_path,{1,2,3});require(!cpu::prepare(external.c_str(),options,error),"short file fails whole model");write(image_path,{1,2,3,4,5,6});require(!cpu::prepare(external.c_str(),options,error),"invalid decoder bytes fail whole model");write(image_path,png);
        cpu::Options dimension=options;dimension.max_texture_size=2;require(!cpu::prepare(root_model.c_str(),dimension,error)&&error.find("dimension")!=std::string::npos,"same decoder dimension limit for root-level bow");require(!cpu::prepare(external.c_str(),dimension,error)&&error.find("dimension")!=std::string::npos,"same decoder dimension budget for URI");
        cpu::Options tiny=options;tiny.cpu_bytes=1024;require(!cpu::prepare(external.c_str(),tiny,error)&&error.find("budget")!=std::string::npos,"whole CPU budget applies to URI model");
        std::uint64_t low=1024,high=384ull*1024*1024;while(low+1<high){const auto mid=(low+high)/2;tiny.cpu_bytes=mid;if(cpu::prepare(external.c_str(),tiny,error))high=mid;else low=mid;}
        tiny.cpu_bytes=high;require(bool(cpu::prepare(external.c_str(),tiny,error)),"exact CPU budget threshold succeeds");tiny.cpu_bytes=high-1;require(!cpu::prepare(external.c_str(),tiny,error)&&error.find("budget")!=std::string::npos,"one byte below full URI preparation budget fails");
        for(int i=1;i<argc;++i){const auto actual=cpu::prepare(argv[i],options,error);require(bool(actual),std::string("Actual bank sample ")+argv[i]+": "+error);require(!actual->textures.empty()&&!actual->meshes.empty(),"actual source sample retained materials and geometry");std::cout<<"Actual bank sample: "<<argv[i]<<" textures="<<actual->textures.size()<<" geometry_bytes="<<actual->geometry_bytes<<" texture_bytes="<<actual->texture_bytes<<'\n';}
        require(texture_cache_stats().references==0&&destroys==5,"all exact-cache ownership references released");texture_cache_shutdown();fs::remove_all(directory);
        std::cout<<"Shared model images CPU PASS: "<<checks<<" checks; embedded/root-level/one-level/two-level/symlinked-model exact bytes and mip pixels/cache IDs; local URI bounds, missing/corrupt files, traversal/network and symlink escapes, decoder/CPU budgets\n";
    }catch(const std::exception &e){texture_cache_shutdown();fs::remove_all(directory);std::cerr<<e.what()<<'\n';return 1;}
}

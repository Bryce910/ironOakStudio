#ifndef VALWOOD_WORLD_MODEL_IMAGE_BYTES_H
#define VALWOOD_WORLD_MODEL_IMAGE_BYTES_H
#ifndef CGLTF_H_INCLUDED__
#include "cgltf.h"
#endif
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

// One encoded-byte contract for worker preparation and synchronous GL loading.
// Generated shared images keep the exact bytes used by the embedded-image cache
// key. This is deliberately not a general glTF URI/network/file URL loader.
namespace world_model_image_bytes {
constexpr std::uint64_t maximum_bytes=64ull*1024*1024;
struct Image {
    const unsigned char *embedded=nullptr;
    std::vector<unsigned char> local;
    std::size_t length=0;
    const unsigned char *data()const{return local.empty()?embedded:local.data();}
    bool external()const{return !local.empty();}
};
inline bool shared_uri(std::string_view uri,unsigned &parents,std::string_view &leaf){
    constexpr std::string_view zero="textures/shared/",one="../textures/shared/",two="../../textures/shared/";
    if(uri.substr(0,two.size())==two){parents=2;leaf=uri.substr(two.size());}
    else if(uri.substr(0,one.size())==one){parents=1;leaf=uri.substr(one.size());}
    else if(uri.substr(0,zero.size())==zero){parents=0;leaf=uri.substr(zero.size());}
    else return false;
    if(leaf.size()!=68||!(leaf.substr(64)==".png"||leaf.substr(64)==".jpg"))return false;
    for(unsigned i=0;i<64;++i)if(!((leaf[i]>='0'&&leaf[i]<='9')||(leaf[i]>='a'&&leaf[i]<='f')))return false;
    return true;
}
inline bool read(const cgltf_image *image,const char *model_path,std::uint64_t external_budget,Image &result,std::string &error){
    error.clear();result=Image{};
    try{
        if(!image)throw "Missing material image";
        if(image->buffer_view){
            if(image->uri)throw "Material image cannot use both embedded bytes and a URI";
            const auto *bytes=cgltf_buffer_view_data(image->buffer_view);const auto length=image->buffer_view->size;
            if(!bytes||length<4||length>maximum_bytes)throw "Invalid embedded material image length";
            result.embedded=bytes;result.length=length;return true;
        }
        unsigned parents=0;std::string_view leaf;
        if(!image->uri||!shared_uri(image->uri,parents,leaf))throw "Unsupported shared material image URI";
        if(!model_path||!*model_path)throw "Missing shared material image model path";
        // Resolve the model first: project models commonly link into the bank.
        // Interpreting .. against the symlink's containing project is wrong.
        auto root=std::filesystem::canonical(std::filesystem::path(model_path)).parent_path();
        while(parents--)root=root.parent_path();
        const auto directory=(root/"textures"/"shared").lexically_normal();
        if(std::filesystem::canonical(directory)!=directory)throw "Shared material image directory may not redirect outside its bank";
        const auto path=directory/std::string(leaf);
        if(std::filesystem::is_symlink(std::filesystem::symlink_status(path))||!std::filesystem::is_regular_file(path)||std::filesystem::canonical(path).parent_path()!=directory)
            throw "Shared material image must be a regular local bank file";
        const auto length=std::filesystem::file_size(path);
        if(length<4||length>maximum_bytes)throw "Invalid shared material image length";
        if(length>external_budget)throw "Shared material image CPU budget exceeded";
        std::ifstream file(path,std::ios::binary|std::ios::ate);
        if(!file||file.tellg()!=std::streamoff(length))throw "Shared material image file changed or could not be opened";
        Image staged;staged.length=static_cast<std::size_t>(length);staged.local.resize(staged.length);
        file.seekg(0);file.read(reinterpret_cast<char *>(staged.local.data()),std::streamsize(length));
        if(!file||file.peek()!=std::char_traits<char>::eof())throw "Shared material image read failed or file changed";
        result=std::move(staged);return true;
    }catch(const char *message){error=message;}catch(const std::exception &e){error=std::string("Shared material image: ")+e.what();}catch(...){error="Unknown material image read failure";}
    result=Image{};return false;
}
}
#endif

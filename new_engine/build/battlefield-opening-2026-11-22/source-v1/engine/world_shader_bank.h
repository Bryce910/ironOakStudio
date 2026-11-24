#ifndef VALWOOD_WORLD_SHADER_BANK_H
#define VALWOOD_WORLD_SHADER_BANK_H
// AAA merge: a generation owns its shader bank. Resolve project-relative
// "shaders/..." paths against VALWOOD_SHADER_BANK_ROOT when the file exists
// there, otherwise keep the original path.
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
inline std::string world_shader_bank_path(const char *path){
    if(!path)return {};
    const char *bank=std::getenv("VALWOOD_SHADER_BANK_ROOT");
    if(bank&&*bank&&std::strncmp(path,"shaders/",8)==0){
        std::string banked=std::string(bank)+"/"+(path+8);
        if(std::ifstream(banked).good())return banked;
    }
    return path;
}
#endif

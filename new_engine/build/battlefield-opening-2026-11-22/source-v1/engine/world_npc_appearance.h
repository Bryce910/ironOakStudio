#ifndef VALWOOD_WORLD_NPC_APPEARANCE_H
#define VALWOOD_WORLD_NPC_APPEARANCE_H
#include "engine/world_npc_memory.h"
#include "engine/world_models.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <cctype>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <vector>
#include <iterator>
namespace world_npc_appearance {
struct Resolved {
    float scale=1;
    WorldModelCharacterLook look={1,0,1,0,{1,1,1},0};
    std::string note;
};
struct Binding{unsigned resident=0;std::string file;};
inline bool valid_bindings(const std::vector<Binding> &items,std::string &error){
    if(items.size()>32){error="At most 32 appearance bindings are supported";return false;}
    for(unsigned i=0;i<items.size();i++){const auto &b=items[i];if(b.resident>=140||b.file.empty()||b.file.size()>511||b.file.find_first_of("\r\n\t")!=std::string::npos||b.file.find('\0')!=std::string::npos){error="Invalid appearance binding";return false;}for(unsigned j=0;j<i;j++)if(items[j].resident==b.resident){error="Duplicate appearance target";return false;}}
    error.clear();return true;
}
inline bool load_bindings(const std::string &path,std::vector<Binding> &out,std::string &error){
    std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in){error="Cannot open appearance bindings";return false;}const auto size=in.tellg();if(size<0||size>32768){error="Appearance bindings exceed 32 KiB";return false;}std::string bytes(std::size_t(size),'\0');in.seekg(0);if(!bytes.empty()&&!in.read(bytes.data(),std::streamsize(bytes.size()))){error="Cannot read appearance bindings";return false;}
    std::istringstream stream(bytes);std::string marker;unsigned version=0,count=0;if(!(stream>>marker>>version>>count)||marker!="VALWOOD_NPC_LOOKS"||version!=1||count>32){error="Invalid appearance bindings header";return false;}
    std::vector<Binding> next(count);for(auto &b:next)if(!(stream>>marker>>b.resident>>std::quoted(b.file))||marker!="BIND"){error="Truncated appearance binding";return false;}
    if(!(stream>>marker)||marker!="END"){error="Missing appearance bindings end marker";return false;}stream>>std::ws;if(!stream.eof()||!valid_bindings(next,error)){if(error.empty())error="Unexpected appearance binding data";return false;}out=std::move(next);error.clear();return true;
}
inline bool save_bindings(const std::string &path,const std::vector<Binding> &items,std::string &error){
    if(!valid_bindings(items,error))return false;
    auto temporary=std::filesystem::path(path);temporary+=".tmp";std::ofstream out(temporary,std::ios::binary|std::ios::trunc);if(!out){error="Cannot write appearance bindings";return false;}out<<"VALWOOD_NPC_LOOKS 1 "<<items.size()<<'\n';for(const auto &b:items)out<<"BIND "<<b.resident<<' '<<std::quoted(b.file)<<'\n';out<<"END\n";out.close();if(!out){error="Cannot finish appearance bindings";return false;}std::error_code ec;std::filesystem::rename(temporary,path,ec);if(ec){error=ec.message();return false;}error.clear();return true;
}
// Explicit Save imports immutable, validated profile snapshots into the project
// so a packaged world never depends on a Downloads path on this computer.
inline bool save_project_bindings(const std::string &root,std::vector<Binding> &items,std::string &error){
    if(!valid_bindings(items,error))return false;
    const auto folder=std::filesystem::path(root)/"ai/memories/appearance_imports";std::error_code ec;std::filesystem::create_directories(folder,ec);if(ec){error=ec.message();return false;}
    auto next=items;
    for(auto &b:next){
        auto source=std::filesystem::path(b.file);if(source.is_relative())source=std::filesystem::path(root)/source;
        world_npc_memory::Bank bank;if(!bank.load(source.u8string().c_str(),error))return false;
        std::ifstream input(source,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(input)),{});if(input.bad()||bytes.size()>2*1024*1024){error="Cannot import profile snapshot";return false;}
        std::uint64_t hash=14695981039346656037ull;for(unsigned char c:bytes){hash^=c;hash*=1099511628211ull;}
        std::ostringstream name;name<<bank.document().profile.npc_id<<'_'<<std::hex<<hash<<".vwmemory";const auto destination=folder/name.str();
        const bool existed=std::filesystem::exists(destination,ec);if(ec){error=ec.message();return false;}
        if(!existed&&!std::filesystem::copy_file(source,destination,std::filesystem::copy_options::none,ec)){error=ec.message();return false;}
        const auto size=std::filesystem::file_size(destination,ec);bool matches=!ec&&size==bytes.size();
        if(matches){std::ifstream check(destination,std::ios::binary);const std::string copied((std::istreambuf_iterator<char>(check)),{});matches=!check.bad()&&copied==bytes;}
        if(!matches){if(!existed)std::filesystem::remove(destination,ec);error="Profile changed during import, or snapshot filename conflicts";return false;}
        b.file=(std::filesystem::path("ai/memories/appearance_imports")/name.str()).generic_string();
    }
    if(!save_bindings((std::filesystem::path(root)/"ai/npc_appearances.vwlooks").string(),next,error))return false;
    items=std::move(next);return true;
}
inline std::string canonical(std::string s){for(char &c:s)if(c>='A'&&c<='Z')c=char(c-'A'+'a');const auto first=s.find_first_not_of(" \t\r\n"),last=s.find_last_not_of(" \t\r\n");return first==std::string::npos?"":s.substr(first,last-first+1);}
inline bool hair_color(const std::string &value,float out[3]){
    const auto name=canonical(value);unsigned rgb=0;bool found=false;
    struct Color{const char *name;unsigned rgb;};
    static constexpr Color palette[]={{"black",0x171414},{"dark brown",0x3b2419},{"brown",0x69402a},{"light brown",0x956442},{"blonde",0xc7a15c},{"blond",0xc7a15c},{"red",0x9d4022},{"auburn",0x713122},{"gray",0x888584},{"grey",0x888584},{"silver",0xbab9b6},{"white",0xe0dcd3}};
    for(const auto &c:palette)if(name==c.name){rgb=c.rgb;found=true;break;}
    if(!found&&name.size()==7&&name[0]=='#'){rgb=0;found=true;for(unsigned i=1;i<7;i++){const char c=name[i];const int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;if(n<0){found=false;break;}rgb=(rgb<<4)|unsigned(n);}}
    if(!found)return false;
    for(unsigned i=0;i<3;i++){const float s=float((rgb>>(16-8*i))&255)/255;out[i]=s<=.04045f?s/12.92f:std::pow((s+.055f)/1.055f,2.4f);}return true;
}
// Resolve once at explicit application/loading boundaries. No inferred skin,
// race, age morph, collision mass or gameplay strength is derived from text.
inline bool resolve(const world_npc_memory::Appearance &a,float reference_height,unsigned capabilities,Resolved &out,std::string &error){
    if(!std::isfinite(reference_height)||reference_height<.1f){error="Missing neutral model height";return false;}
    Resolved r;
    if(a.height_cm){if(!std::isfinite(*a.height_cm)||*a.height_cm<1||*a.height_cm>1000){error="Invalid height";return false;}r.scale=float(*a.height_cm*.01/reference_height);}
    const auto build=canonical(a.build);
    if(build=="slim"||build=="lean")r.look.width=.90f;
    else if(build=="athletic")r.look.width=1.04f;
    else if(build=="sturdy"||build=="stocky"||build=="broad")r.look.width=1.14f;
    else if(build=="heavy")r.look.width=1.23f;
    else if(!build.empty()&&build!="average")r.note+="Unrecognized build stays descriptive. ";
    if(a.weight_kg){if(!std::isfinite(*a.weight_kg)||*a.weight_kg<.1||*a.weight_kg>10000){error="Invalid weight";return false;}const double height=reference_height*r.scale;const double mass=70*std::pow(height/1.75,3);r.look.width*=float(std::clamp(std::sqrt(*a.weight_kg/mass),.8,1.25));}
    r.look.width=std::clamp(r.look.width,.65f,1.45f);
    if(capabilities&WORLD_MODEL_CHARACTER_HAIR){
        if(!a.hair_color.empty()){if(hair_color(a.hair_color,r.look.hair_tint))r.look.color_strength=1;else r.note+="Hair color needs a supported name or #RRGGBB. ";}
        if(a.hair_density=="bald")r.look.hide_hair=1;
        if(capabilities&WORLD_MODEL_CHARACTER_MASKED_HAIR){
            if(a.hair_density=="sparse")r.look.cutoff_offset=.13f;else if(a.hair_density=="dense")r.look.cutoff_offset=-.08f;
            if(a.hair_thickness=="fine")r.look.alpha_exponent=1.2f;else if(a.hair_thickness=="coarse")r.look.alpha_exponent=.8f;
        }else if(!a.hair_thickness.empty()||(!a.hair_density.empty()&&a.hair_density!="bald"))r.note+="This solid hair mesh supports color/baldness; density and strand coverage require alpha hair cards. ";
    }else if(!a.hair_color.empty()||!a.hair_density.empty()||!a.hair_thickness.empty())r.note+="This model has no separate supported hair material. ";
    r.note+="Height uses the neutral pose envelope. Build/weight change cosmetic body width. Skin, eyes, age, hairstyles, facial hair and scars remain descriptive; they need authored masks, meshes or morphs.";
    out=std::move(r);error.clear();return true;
}
}
#endif

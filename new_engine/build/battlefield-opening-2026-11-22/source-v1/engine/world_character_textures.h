#ifndef VALWOOD_WORLD_CHARACTER_TEXTURES_H
#define VALWOOD_WORLD_CHARACTER_TEXTURES_H
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>
// Immutable, loading-boundary texture variants. Key the exact original encoded
// bytes, including embedded hero maps; arbitrary world materials are untouched.
// No reads, allocation, or GL operations occur during normal character draws.
namespace world_character_textures {
struct File {std::string path;std::uint64_t bytes=0,hash=0;};
constexpr unsigned profile_count=4,inherit_quality=profile_count;
struct Entry {std::array<File,profile_count> files;};
inline unsigned quality=1;
inline thread_local unsigned loading_quality=inherit_quality;
struct LoadingQuality {
 unsigned previous;
 explicit LoadingQuality(unsigned selected):previous(loading_quality){loading_quality=selected;}
 ~LoadingQuality(){loading_quality=previous;}
};
inline std::filesystem::path root;
inline std::map<std::pair<std::uint64_t,std::uint64_t>,Entry> entries;
inline std::uint64_t fingerprint(const void *data,std::size_t count){
 auto *p=static_cast<const unsigned char *>(data);std::uint64_t h=14695981039346656037ull;
 for(std::size_t i=0;i<count;++i){h^=p[i];h*=1099511628211ull;}return h;
}
// Curated owned bare-hand color identities: the imported 1K source and
// accepted High2K derivative. Classify before replacement so Ultra cannot
// silently turn off the established hand-material response.
inline bool known_hand_color(std::uint64_t hash,std::size_t bytes){
 return (hash==2007086948057565833ull&&bytes==218182)||
        (hash==0x69fbf024839f6162ull&&bytes==4371709);
}
// Compact/static character imports keep Ultra color while retaining the
// accepted High surface maps. Skeletal imports have their own aggregate
// 768MiB reference budget and select registered fallback images there.
inline unsigned model_profile(unsigned requested,bool color){return requested==3&&!color?2u:requested;}
inline bool hand_response(unsigned profile,std::uint64_t original_hash,std::size_t original_bytes,
                          std::uint64_t selected_hash,std::size_t selected_bytes){
 // The old tiers retain exactly their accepted High derivative contract.
 // Only Ultra additionally follows the curated original before replacement.
 return (selected_hash==0x69fbf024839f6162ull&&selected_bytes==4371709)||
        (profile==3&&known_hand_color(original_hash,original_bytes));
}
inline bool configure(const char *directory,unsigned level,std::string &error){
 error.clear();if(level>=profile_count){error="Invalid character texture quality";return false;}
 try{
  std::map<std::pair<std::uint64_t,std::uint64_t>,Entry> staged;
  auto target=std::filesystem::weakly_canonical(directory);
  auto manifest=target/"textures.manifest";
  if(!std::filesystem::exists(manifest)){entries.clear();root=target;quality=level;return true;}
  if(std::filesystem::file_size(manifest)>2*1024*1024){error="Character texture manifest too large";return false;}
  std::ifstream file(manifest);std::string header;unsigned version=0;
  if(!(file>>header>>version)||header!="VALWOOD_CHARACTER_TEXTURES"||(version!=1&&version!=2)){error="Invalid character texture manifest";return false;}
  while(file>>std::ws&&!file.eof()){
   std::uint64_t hash=0,bytes=0;Entry entry;
   if(!(file>>hash>>bytes)||bytes<4||bytes>64*1024*1024){error="Invalid source texture key";return false;}
   const unsigned columns=version==1?3u:profile_count;
   for(unsigned i=0;i<columns;++i){auto &f=entry.files[i];
    if(!(file>>std::quoted(f.path)>>f.bytes>>f.hash)||f.bytes<4||f.bytes>64*1024*1024){error="Invalid character texture variant";return false;}
    const auto p=std::filesystem::path(f.path);
    if(p.is_absolute()||p.empty()){error="Invalid character texture path";return false;}
    for(const auto &part:p)if(part==".."||part=="."){error="Character texture path escapes asset root";return false;}
    const auto resolved=std::filesystem::canonical(target/p);
    if(resolved!=(target/p).lexically_normal()||std::filesystem::is_symlink(target/p)||!std::filesystem::is_regular_file(resolved)||std::filesystem::file_size(resolved)!=f.bytes){error="Missing or changed character texture file: "+f.path;return false;}
   }
   if(version==1)entry.files[3]=entry.files[2]; // Legacy banks retain exact High bytes for Ultra.
   if(staged.size()>=2048||!staged.emplace(std::make_pair(hash,bytes),std::move(entry)).second){error="Duplicate/excessive character texture entries";return false;}
  }
  entries=std::move(staged);root=target;quality=level;return true;
 }catch(const std::exception &e){error=std::string("Character texture configuration: ")+e.what();return false;}
}
inline bool replace(const void *bytes,std::size_t length,std::vector<unsigned char>&out,std::string &error){
 out.clear();error.clear();if(entries.empty())return true;
 const auto found=entries.find({fingerprint(bytes,length),length});if(found==entries.end())return true;
 const auto &f=found->second.files[loading_quality<profile_count?loading_quality:quality];
 // Identical variants retain the exact encoded source without another read.
 if(f.bytes==length&&f.hash==found->first.first)return true;
 try{
  std::ifstream file(root/f.path,std::ios::binary|std::ios::ate);
  if(!file||file.tellg()!=std::streamoff(f.bytes)){error="Cannot read character texture: "+f.path;return false;}
  out.resize(std::size_t(f.bytes));file.seekg(0);file.read(reinterpret_cast<char *>(out.data()),std::streamsize(out.size()));
  if(!file||file.peek()!=std::char_traits<char>::eof()||fingerprint(out.data(),out.size())!=f.hash){out.clear();error="Character texture integrity failed: "+f.path;return false;}
  return true;
 }catch(const std::exception &e){out.clear();error=std::string("Character texture read: ")+e.what();return false;}
}
}
#endif

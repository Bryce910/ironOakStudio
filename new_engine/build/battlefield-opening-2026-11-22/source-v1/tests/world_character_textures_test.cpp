#include "engine/world_character_textures.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <iterator>
namespace bank=world_character_textures;
static std::string read(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static void write(const std::filesystem::path &p,const std::string &b){std::ofstream f(p,std::ios::binary|std::ios::trunc);f<<b;assert(f);}
int main(){
 char temp[]="/tmp/valwood-character-textures-XXXXXX";const char *folder=mkdtemp(temp);assert(folder);const std::filesystem::path root=folder;
 const std::array<std::string,4> raw={"LOW-MAP-BYTES","MEDIUM-MAP-BYTES","HIGH-MAP-BYTES","ULTRA-NATIVE-4K-MAP-BYTES"};
 const std::string source="ORIGINAL-OWNED-MAP";const auto identity=bank::fingerprint(source.data(),source.size());
 for(unsigned i=0;i<raw.size();++i)write(root/(std::to_string(i)+".png"),raw[i]);
 auto row=[&](unsigned count){std::ostringstream s;s<<identity<<' '<<source.size();for(unsigned i=0;i<count;++i)s<<' '<<std::quoted(std::to_string(i)+".png")<<' '<<raw[i].size()<<' '<<bank::fingerprint(raw[i].data(),raw[i].size());return s.str()+"\n";};
 const auto v1=std::string("VALWOOD_CHARACTER_TEXTURES 1\n")+row(3),v2=std::string("VALWOOD_CHARACTER_TEXTURES 2\n")+row(4);
 std::string error;std::vector<unsigned char> selected;
 write(root/"textures.manifest",v2);unsigned selections=0,rejections=0;
 for(unsigned level=0;level<4;++level){
  if(!bank::configure(root.c_str(),level,error)){std::fprintf(stderr,"bank level%u: %s\n",level,error.c_str());return 1;}assert(error.empty());assert(bank::quality==level&&bank::entries.size()==1&&bank::loading_quality==bank::inherit_quality);
  assert(bank::replace(source.data(),source.size(),selected,error));assert(std::string(selected.begin(),selected.end())==raw[level]);++selections;
 }
 for(unsigned level=0;level<4;++level){
  assert(bank::model_profile(level,true)==level);
  assert(bank::model_profile(level,false)==(level==3?2u:level));
  bank::LoadingQuality role(bank::model_profile(level,false));
  assert(bank::replace(source.data(),source.size(),selected,error));assert(std::string(selected.begin(),selected.end())==raw[level==3?2u:level]);
 }
 // Index3 is an actual loading override; the previous inherit sentinel3
 // would silently use globalLow here. Nested scope restoration is exact.
 assert(bank::configure(root.c_str(),0,error));
 {bank::LoadingQuality ultra(3);assert(bank::replace(source.data(),source.size(),selected,error));assert(std::string(selected.begin(),selected.end())==raw[3]);
  {bank::LoadingQuality low(0);assert(bank::replace(source.data(),source.size(),selected,error));assert(std::string(selected.begin(),selected.end())==raw[0]);}
  assert(bank::loading_quality==3);
 }assert(bank::loading_quality==bank::inherit_quality&&bank::quality==0);
 write(root/"textures.manifest",v1);assert(bank::configure(root.c_str(),3,error));assert(bank::quality==3);
 assert(bank::replace(source.data(),source.size(),selected,error));assert(std::string(selected.begin(),selected.end())==raw[2]);++selections;
 {bank::LoadingQuality ultra(3);assert(bank::replace(source.data(),source.size(),selected,error));assert(std::string(selected.begin(),selected.end())==raw[2]);}
 assert(bank::entries.begin()->second.files[3].path==bank::entries.begin()->second.files[2].path);
 auto reject=[&](const std::string &manifest){
  const auto before_root=bank::root;const auto before_quality=bank::quality;const auto before_count=bank::entries.size();const auto before_file=bank::entries.begin()->second.files[3].path;
  write(root/"textures.manifest",manifest);assert(!bank::configure(root.c_str(),3,error)&&!error.empty());
  assert(bank::root==before_root&&bank::quality==before_quality&&bank::entries.size()==before_count&&bank::entries.begin()->second.files[3].path==before_file);++rejections;
 };
 reject("VALWOOD_CHARACTER_TEXTURES 3\n"+row(4));reject("WRONG_HEADER 2\n"+row(4));
 reject("VALWOOD_CHARACTER_TEXTURES 2\n"+row(3));reject("VALWOOD_CHARACTER_TEXTURES 1\n"+row(4));reject(v2+row(4));reject(v2+"trailing junk\n");
 auto replace=[](std::string text,const std::string &from,const std::string &to){const auto at=text.find(from);assert(at!=std::string::npos);text.replace(at,from.size(),to);return text;};
 reject(replace(v2,"\"3.png\"","\"/etc/passwd\""));reject(replace(v2,"\"3.png\"","\"../3.png\""));reject(replace(v2,"\"3.png\"","\"./3.png\""));reject(replace(v2,"\"3.png\"","\"missing.png\""));
 std::filesystem::create_symlink(root/"3.png",root/"linked.png");reject(replace(v2,"\"3.png\"","\"linked.png\""));
 std::filesystem::create_directory(root/"inside");write(root/"inside/real.png",raw[3]);std::filesystem::create_directory_symlink(root/"inside",root/"alias");reject(replace(v2,"\"3.png\"","\"alias/real.png\""));
 reject(std::string(2*1024*1024+1,'x'));
 write(root/"textures.manifest",v2);assert(bank::configure(root.c_str(),3,error));assert(!bank::configure(root.c_str(),4,error)&&bank::quality==3);
 const auto expected=read(root/"3.png");write(root/"3.png",std::string(expected.size(),'x'));assert(!bank::replace(source.data(),source.size(),selected,error)&&selected.empty());write(root/"3.png",expected);
 assert(bank::replace(source.data(),source.size(),selected,error)&&std::string(selected.begin(),selected.end())==expected);
 const std::string unrelated="UNREGISTERED-MAP";assert(bank::replace(unrelated.data(),unrelated.size(),selected,error)&&selected.empty());
 // Exact source contracts, not filenames or an arbitrary registry entry,
 // preserve the accepted bare-hand response across every replacement tier.
 assert(bank::known_hand_color(2007086948057565833ull,218182));assert(bank::known_hand_color(0x69fbf024839f6162ull,4371709));
 for(unsigned profile=0;profile<3;++profile){
  assert(!bank::hand_response(profile,2007086948057565833ull,218182,2007086948057565833ull,218182));
  assert(bank::hand_response(profile,2007086948057565833ull,218182,0x69fbf024839f6162ull,4371709));
 }
 assert(bank::hand_response(3,2007086948057565833ull,218182,1234,9999));
 assert(!bank::hand_response(3,2007086948057565832ull,218182,1234,9999));
 assert(!bank::known_hand_color(2007086948057565832ull,218182));assert(!bank::known_hand_color(2007086948057565833ull,218183));
 std::filesystem::remove(root/"textures.manifest");assert(bank::configure(root.c_str(),3,error)&&bank::entries.empty()&&bank::quality==3);
 std::filesystem::remove_all(root);
 std::printf("PASS character textures: %u native/exact variant selections, %u malformed/path/corruption cases; Ultra index3, inherit4, v1High fallback, scoped restoration and curated hand provenance\n",selections,rejections+2);
}

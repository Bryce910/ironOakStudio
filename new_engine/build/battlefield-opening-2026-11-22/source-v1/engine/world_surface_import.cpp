#include "engine/world_platform.h"
#include "engine/world_surface_import.h"
#include "stb_image.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <unistd.h>
namespace world_surface {
ImportResult import_image(const std::string &project,const std::string &source,ImageRole role){
 ImportResult result;
 try{
  if(unsigned(role)>2)throw "Invalid image role";
  auto size=std::filesystem::file_size(source);
  if(!size||size>32*1024*1024)throw "Use a source image below32MiB";
  int width=0,height=0,channels=0;
  if(stbi_is_hdr(source.c_str())||!stbi_info(source.c_str(),&width,&height,&channels)||width<1||height<1||width>8192||height>8192||std::uint64_t(width)*height>16*1024*1024)throw "Use a standard image with at most16million pixels (8192px maximum dimension)";
  std::unique_ptr<unsigned char,decltype(&stbi_image_free)> pixels(stbi_load(source.c_str(),&width,&height,&channels,4),stbi_image_free);
  if(!pixels)throw "Cannot decode source image";
  pixels.reset();
  std::ifstream input(source,std::ios::binary);std::vector<char> bytes(size);
  if(!input.read(bytes.data(),std::streamsize(size)))throw "Cannot read source image";
  std::uint64_t hash=14695981039346656037ull;
  for(unsigned char b:bytes){hash^=b;hash*=1099511628211ull;}
  char name[48];std::snprintf(name,sizeof name,"image_%016llx_%u.img",(unsigned long long)hash,unsigned(role));
  auto root=std::filesystem::canonical(project),directory=std::filesystem::weakly_canonical(root/"surfaces");
  if(directory.generic_string().rfind(root.generic_string()+"/",0)!=0)throw "Surface folder escapes project";
  std::filesystem::create_directories(directory);auto path=directory/name;
  if(std::filesystem::exists(path)){
   std::ifstream previous(path,std::ios::binary|std::ios::ate);
   if(!previous||previous.tellg()!=std::streamoff(size))throw "Existing surface revision differs";
   previous.seekg(0);std::vector<char> old(size);
   if(!previous.read(old.data(),std::streamsize(size))||old!=bytes)throw "Existing surface revision differs";
  }else{
   auto pattern=(directory/".import-XXXXXX").string();std::vector<char> temporary(pattern.begin(),pattern.end());temporary.push_back(0);
   int fd=mkstemp(temporary.data());if(fd<0)throw "Cannot create surface transaction";
   size_t done=0;bool ok=true;
   while(done<bytes.size()){auto n=write(fd,bytes.data()+done,bytes.size()-done);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}done+=size_t(n);}
   if(ok)ok=fsync(fd)==0;
   int closed=close(fd);ok=ok&&closed==0;
   if(ok)ok=link(temporary.data(),path.c_str())==0;
   unlink(temporary.data());if(!ok)throw "Surface save transaction failed";
  }
  result.image.name=std::filesystem::path(source).stem().string().substr(0,48);
  for(char &c:result.image.name)if(static_cast<unsigned char>(c)<32||c==127)c='_';
  if(result.image.name.empty())result.image.name="Image";
  result.image.path="surfaces/"+std::string(name);result.image.role=role;
  result.width=unsigned(width);result.height=unsigned(height);result.ok=true;
 }catch(const char *error){result.error=error;}catch(const std::exception &error){result.error=error.what();}
 return result;
}
}

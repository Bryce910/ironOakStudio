#include "engine/world_surface_import.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <unistd.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
int main(){
 namespace fs=std::filesystem;
 char directory[]="/tmp/valwood-surface-import-XXXXXX";assert(mkdtemp(directory));fs::path root=directory;fs::create_directory(root/"project");
 fs::path source=root/"source.ppm";const std::string bytes="P6\n2 2\n255\n"+std::string(12,'\x7f');
 {std::ofstream f(source,std::ios::binary);f.write(bytes.data(),bytes.size());}
 auto imported=world_surface::import_image((root/"project").string(),source.string(),world_surface::ImageRole::Color);
 assert(imported.ok&&imported.width==2&&imported.height==2&&imported.image.path.rfind("surfaces/",0)==0);
 std::ifstream f(root/"project"/imported.image.path,std::ios::binary);std::string copied((std::istreambuf_iterator<char>(f)),{});assert(copied==bytes);
 auto repeated=world_surface::import_image((root/"project").string(),source.string(),world_surface::ImageRole::Color);assert(repeated.ok&&repeated.image.path==imported.image.path);
 assert(!world_surface::import_image((root/"project").string(),source.string(),world_surface::ImageRole(99)).ok);
 {std::ofstream bad(root/"broken.png");bad<<"not an image";}
 assert(!world_surface::import_image((root/"project").string(),(root/"broken.png").string(),world_surface::ImageRole::Normal).ok);
 fs::create_directory(root/"escaped");fs::create_directory(root/"second");fs::create_directory_symlink(root/"escaped",root/"second/surfaces");
 assert(!world_surface::import_image((root/"second").string(),source.string(),world_surface::ImageRole::Color).ok);
 {std::ofstream corrupt(root/"project"/imported.image.path,std::ios::binary);corrupt<<"modified";}
 assert(!world_surface::import_image((root/"project").string(),source.string(),world_surface::ImageRole::Color).ok);
 std::ifstream original(source,std::ios::binary);copied.assign(std::istreambuf_iterator<char>(original),{});assert(copied==bytes);
 fs::remove_all(root);std::puts("PASS immutable surface import: decode/role validation, duplicate reuse, original bytes, conflicting revision and folder-escape rejection");
}

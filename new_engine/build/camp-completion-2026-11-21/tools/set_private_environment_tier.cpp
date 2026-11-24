#include "engine/world_graphics_settings.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
int main(int argc,char **argv){
 if(argc!=3)return 2;
 auto path=std::filesystem::weakly_canonical(argv[1]);
 if(path.string().find("/build/camp-completion-2026-11-21/review/")==std::string::npos)return 3;
 WorldGraphicsSettings s;char error[256];
 if(world_graphics_settings_load(path.c_str(),&s,error,sizeof error)!=WorldGraphicsSettingsLoadResult::Loaded){std::fprintf(stderr,"%s\n",error);return 4;}
 const char *names[]={"Low","Medium","High","Ultra"};int level=-1;
 for(int i=0;i<4;i++)if(!std::strcmp(names[i],argv[2]))level=i;
 if(level<0)return 5;
 s.environment_textures=WorldEnvironmentTextures(level);
 if(!world_graphics_settings_save(path.c_str(),s,error,sizeof error)){std::fprintf(stderr,"%s\n",error);return 6;}
 std::printf("Private scenery texture tier: %s\n",names[level]);return 0;
}

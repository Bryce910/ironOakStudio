#include "engine/asset_system.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "stb_image.h"
#include "gl_helpers.h"

#define ASSET_MANIFEST_FILE "cooked/asset_manifest.csv"
#define MAX_COOKED_ASSETS 4096

typedef struct{
    char source[512];
    char basename[256];
    char cooked[512];
}AssetManifestEntry;

typedef struct{
    bool loaded;
    bool tried;
    int count;
    AssetManifestEntry entries[MAX_COOKED_ASSETS];
}AssetManifest;

static AssetManifest g_asset_manifest={0};

void path_normalize_slashes(char *path){
    if(!path)return;
    for(;*path;path++) if(*path=='\\') *path='/';
}

bool path_has_extension(const char *path){
    if(!path||!path[0])return false;
    const char *slash1=strrchr(path,'/');
    const char *slash2=strrchr(path,'\\');
    const char *slash=slash1>slash2?slash1:slash2;
    const char *dot=strrchr(path,'.');
    return dot&&(!slash||dot>slash)&&dot[1];
}

const char *path_basename_ptr(const char *path){
    if(!path)return "";
    {
        const char *s1=strrchr(path,'/');
        const char *s2=strrchr(path,'\\');
        const char *s=s1>s2?s1:s2;
        return s?s+1:path;
    }
}

void ufbx_string_copy(char *dst,size_t cap,ufbx_string str){
    if(!dst||cap==0)return;
    {
        size_t n=str.length<cap-1?str.length:cap-1;
        memcpy(dst,str.data,n);
        dst[n]=0;
    }
}

void ufbx_string_to_cstr(char *dst,size_t cap,ufbx_string str){
    if(!dst||cap==0)return;
    {
        size_t n=str.length<cap-1?str.length:cap-1;
        memcpy(dst,str.data,n);
        dst[n]=0;
    }
}

static void asset_manifest_load(void){
    if(g_asset_manifest.tried)return;
    g_asset_manifest.tried=true;
    {
        FILE *f=fopen(ASSET_MANIFEST_FILE,"r");
        if(!f){
            printf("[Assets] No cooked manifest at %s, using raw assets\n",ASSET_MANIFEST_FILE);
            return;
        }
        {
            char line[1200];
            while(fgets(line,sizeof(line),f)&&g_asset_manifest.count<MAX_COOKED_ASSETS){
                char *src=line;
                char *sep=strchr(src,'|');
                char *dst_field;
                char *extra;
                if(!sep)continue;
                *sep++=0;
                dst_field=sep;
                extra=strchr(dst_field,'|');
                if(extra)*extra=0;
                {
                    char *nl=strpbrk(dst_field,"\r\n");
                    if(nl)*nl=0;
                }
                if(!src[0]||!dst_field[0])continue;
                {
                    AssetManifestEntry *e=&g_asset_manifest.entries[g_asset_manifest.count++];
                    snprintf(e->source,sizeof(e->source),"%s",src);
                    snprintf(e->cooked,sizeof(e->cooked),"%s",dst_field);
                    path_normalize_slashes(e->source);
                    path_normalize_slashes(e->cooked);
                    snprintf(e->basename,sizeof(e->basename),"%s",path_basename_ptr(e->source));
                }
            }
        }
        fclose(f);
    }
    g_asset_manifest.loaded=g_asset_manifest.count>0;
    printf("[Assets] Loaded %d cooked asset mappings\n",g_asset_manifest.count);
}

bool asset_resolve_path_copy(char *dst,size_t cap,const char *path){
    if(!dst||cap==0)return false;
    dst[0]=0;
    if(!path||!path[0])return false;

    {
        char normalized[1024];
        snprintf(normalized,sizeof(normalized),"%s",path);
        path_normalize_slashes(normalized);

        asset_manifest_load();
        for(int i=0;i<g_asset_manifest.count;i++){
            AssetManifestEntry *e=&g_asset_manifest.entries[i];
            if(strcmp(e->source,normalized)==0&&access(e->cooked,F_OK)==0){
                snprintf(dst,cap,"%s",e->cooked);
                return true;
            }
        }
        {
            const char *base=path_basename_ptr(normalized);
            for(int i=0;i<g_asset_manifest.count;i++){
                AssetManifestEntry *e=&g_asset_manifest.entries[i];
                if(strcmp(e->basename,base)==0&&access(e->cooked,F_OK)==0){
                    snprintf(dst,cap,"%s",e->cooked);
                    return true;
                }
            }
            if(access(normalized,F_OK)==0){
                snprintf(dst,cap,"%s",normalized);
                return true;
            }
            if(base[0]&&access(base,F_OK)==0){
                snprintf(dst,cap,"%s",base);
                return true;
            }
            snprintf(dst,cap,"%s",normalized);
        }
    }
    return false;
}

uint8_t *asset_stbi_load(const char *path,int *w,int *h,int *ch,int desired_channels){
    char resolved[1024];
    asset_resolve_path_copy(resolved,sizeof(resolved),path);
    return stbi_load(resolved[0]?resolved:path,w,h,ch,desired_channels);
}

void texture_path_resolve(char *dst,size_t cap,const char *model_path,ufbx_string rel){
    char rel_path[512]={0};
    char rel_base[256]={0};
    char model_dir[1024]={0};
    char model_base[256]={0};
    const char *slash;
    const char *base;
    const char *dot;
    if(!dst||cap==0)return;
    dst[0]=0;
    if(rel.length==0)return;

    ufbx_string_copy(rel_path,sizeof(rel_path),rel);
    path_normalize_slashes(rel_path);
    snprintf(rel_base,sizeof(rel_base),"%s",path_basename_ptr(rel_path));
    if(rel_path[0]=='/'||(rel_path[0]&&rel_path[1]==':')){
        asset_resolve_path_copy(dst,cap,rel_path);
        return;
    }

    snprintf(model_dir,sizeof(model_dir),"%s",model_path?model_path:"");
    path_normalize_slashes(model_dir);
    slash=strrchr(model_dir,'/');
    base=path_basename_ptr(model_dir);
    snprintf(model_base,sizeof(model_base),"%s",base);
    dot=strrchr(model_base,'.');
    if(dot) model_base[(size_t)(dot-model_base)]=0;
    if(slash){
        size_t dir_len=(size_t)(slash-model_dir+1);
        snprintf(dst,cap,"%.*s%s",(int)dir_len,model_dir,rel_path);
    }else{
        snprintf(dst,cap,"%s",rel_path);
    }
    asset_resolve_path_copy(dst,cap,dst);
    if(access(dst,F_OK)!=0&&slash&&model_base[0]){
        char alt[1024];
        size_t dir_len=(size_t)(slash-model_dir+1);
        snprintf(alt,sizeof(alt),"%.*s%s/%s",(int)dir_len,model_dir,model_base,rel_path);
        asset_resolve_path_copy(dst,cap,alt);
        if(access(dst,F_OK)!=0&&rel_base[0]){
            snprintf(alt,sizeof(alt),"%.*s%s.fbm/%s",(int)dir_len,model_dir,model_base,rel_base);
            asset_resolve_path_copy(dst,cap,alt);
        }
        if(access(dst,F_OK)!=0&&rel_base[0]){
            snprintf(alt,sizeof(alt),"%.*stextures/%s",(int)dir_len,model_dir,rel_base);
            asset_resolve_path_copy(dst,cap,alt);
        }
        if(access(dst,F_OK)!=0&&rel_base[0]){
            snprintf(alt,sizeof(alt),"%.*s%s",(int)dir_len,model_dir,rel_base);
            asset_resolve_path_copy(dst,cap,alt);
        }
    }
}

GLuint texture_file_try_candidates_mode(const char *path,bool srgb){
    char candidate[1024];
    const char *exts[]={"png","jpg","jpeg","tga","bmp",NULL};
    GLuint id=0;
    if(!path||!path[0])return 0;

    snprintf(candidate,sizeof(candidate),"%s",path);
    path_normalize_slashes(candidate);

    {
        char resolved[1024];
        asset_resolve_path_copy(resolved,sizeof(resolved),candidate);
        if(access(resolved,F_OK)==0){
            id=srgb?GLH_texture_file_srgb(resolved):GLH_texture_file(resolved);
            if(id)return id;
        }

        {
            const char *base=path_basename_ptr(candidate);
            asset_resolve_path_copy(resolved,sizeof(resolved),base);
            if(base[0]&&access(resolved,F_OK)==0){
                id=srgb?GLH_texture_file_srgb(resolved):GLH_texture_file(resolved);
                if(id)return id;
            }

            if(path_has_extension(candidate))return 0;

            for(int ei=0;exts[ei];ei++){
                char try_path[1100];
                snprintf(try_path,sizeof(try_path),"%s.%s",candidate,exts[ei]);
                asset_resolve_path_copy(resolved,sizeof(resolved),try_path);
                if(access(resolved,F_OK)==0){
                    id=srgb?GLH_texture_file_srgb(resolved):GLH_texture_file(resolved);
                    if(id)return id;
                }
                if(base[0]){
                    char try_base[600];
                    snprintf(try_base,sizeof(try_base),"%s.%s",base,exts[ei]);
                    asset_resolve_path_copy(resolved,sizeof(resolved),try_base);
                    if(access(resolved,F_OK)==0){
                        id=srgb?GLH_texture_file_srgb(resolved):GLH_texture_file(resolved);
                        if(id)return id;
                    }
                }
            }
        }
    }
    return 0;
}

GLuint texture_file_try_candidates(const char *path){
    return texture_file_try_candidates_mode(path,true);
}

bool path_contains_ci(const char *path,const char *needle){
    if(!path||!needle||!*needle)return false;
    {
        size_t nlen=strlen(needle);
        for(const char *p=path;*p;p++){
            size_t i=0;
            while(i<nlen&&p[i]&&tolower((unsigned char)p[i])==tolower((unsigned char)needle[i]))i++;
            if(i==nlen)return true;
        }
    }
    return false;
}

bool is_direct_knight_asset(const char *path){
    const char *base=path_basename_ptr(path);
    return path_contains_ci(base,"knight.fbx")||path_contains_ci(base,"knight (copy).fbx");
}

bool is_knight_copy_asset(const char *path){
    const char *base=path_basename_ptr(path);
    return path_contains_ci(base,"knight (copy).fbx");
}

GLuint try_texture_candidates(const char *const *candidates,size_t count){
    for(size_t i=0;i<count;i++){
        GLuint id=texture_file_try_candidates(candidates[i]);
        if(id)return id;
    }
    return 0;
}

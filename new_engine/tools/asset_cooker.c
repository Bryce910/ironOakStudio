#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#if defined(_WIN32)
#include <direct.h>
#define mkdir_one(path) _mkdir(path)
#else
#include <unistd.h>
#define mkdir_one(path) mkdir(path,0755)
#endif

#include "ufbx.h"
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define MAX_PATH_LEN 1024
#define MAX_ASSETS 16384
#define MAX_DEPS_PER_ASSET 64
#define MAX_BONES 64
#define MAX_WEIGHTS 4
#define COOKED_FBX_MAGIC 0x31424643u
#define MODELC_MAGIC 0x3143444du
#define MODELC_VERSION 1u
#define MODELC_VERSION_ANIM 2u
#define MODELC_TEX_PATH_MAX 256u

typedef struct{
    uint32_t magic;
    uint32_t version;
    uint32_t source_path_len;
    uint32_t reserved;
    uint64_t payload_size;
}CookedFbxHeader;

typedef struct{
    uint32_t magic;
    uint32_t version;
    uint32_t mesh_count;
    uint32_t reserved;
    float model_scale,cx,cy,cz,ground_offset,ymin_raw,ymax_raw;
}ModelcHeader;

typedef struct{
    float pos[3],normal[3],uv[2];
    float weights[4];
    int32_t bones[4];
    float tangent[3],bitangent[3];
}ModelcVertex;

typedef struct{
    uint32_t vertex_count;
    uint32_t index_count;
    float geometry_to_world[16];
    uint32_t node_typed_id;
    uint32_t has_pbr;
    float mat_color[4];
    float emissive[3];
    float metallic,roughness,alpha_cutoff;
    char texture_path[MODELC_TEX_PATH_MAX];
    char normal_path[MODELC_TEX_PATH_MAX];
    char mr_path[MODELC_TEX_PATH_MAX];
    char emissive_path[MODELC_TEX_PATH_MAX];
}ModelcMeshHeader;

typedef struct{
    uint32_t vertex_count;
    uint32_t index_count;
    float geometry_to_world[16];
    uint32_t node_typed_id;
    uint32_t has_pbr;
    int32_t skin_index;
    int32_t num_gpu_bones;
    float mat_color[4];
    float emissive[3];
    float metallic,roughness,alpha_cutoff;
    char texture_path[MODELC_TEX_PATH_MAX];
    char normal_path[MODELC_TEX_PATH_MAX];
    char mr_path[MODELC_TEX_PATH_MAX];
    char emissive_path[MODELC_TEX_PATH_MAX];
}ModelcMeshHeaderV2;

typedef struct{
    uint32_t node_count;
    uint32_t skin_count;
    uint32_t anim_count;
    uint32_t reserved;
}ModelcRigHeader;

typedef struct{
    int32_t parent_index;
    float t[3];
    float s[3];
    float r[4];
    float rest_world[16];
}ModelcNodeRecord;

typedef struct{
    int32_t joint_count;
    int32_t joint_nodes[MAX_BONES];
    float inverse_bind[MAX_BONES][16];
}ModelcSkinRecord;

typedef struct{
    char name[64];
    float duration;
    uint32_t channel_count;
    uint32_t reserved;
}ModelcAnimRecord;

typedef struct{
    uint32_t node_index;
    uint32_t path;
    uint32_t interpolation;
    uint32_t key_count;
}ModelcAnimChannelRecord;

typedef struct{
    ufbx_vec3 *normals;
    uint32_t *indices;
    size_t num_normals;
}CookMeshNormals;

typedef enum{
    ASSET_MODEL,
    ASSET_TEXTURE,
    ASSET_AUDIO,
    ASSET_SHADER,
    ASSET_DATA,
    ASSET_TEXT,
    ASSET_UNKNOWN
}AssetKind;

typedef struct{
    char source[MAX_PATH_LEN];
    char cooked[MAX_PATH_LEN];
    char meta[MAX_PATH_LEN];
    char hash[17];
    AssetKind kind;
    int deps[MAX_DEPS_PER_ASSET];
    int dep_count;
    bool imported;
}CookedAsset;

typedef struct{
    char out_root[MAX_PATH_LEN];
    FILE *manifest;
    CookedAsset assets[MAX_ASSETS];
    int asset_count;
}CookContext;

static void path_normalize(char *path){
    for(;*path;path++) if(*path=='\\') *path='/';
}

static const char *path_basename_ptr(const char *path){
    const char *s1=strrchr(path,'/');
    const char *s2=strrchr(path,'\\');
    const char *s=s1>s2?s1:s2;
    return s?s+1:path;
}

static bool path_has_extension(const char *path){
    const char *slash1=strrchr(path,'/');
    const char *slash2=strrchr(path,'\\');
    const char *slash=slash1>slash2?slash1:slash2;
    const char *dot=strrchr(path,'.');
    return dot&&(!slash||dot>slash)&&dot[1];
}

static void path_dirname_copy(char *dst,size_t cap,const char *path){
    const char *slash;
    if(!dst||cap==0)return;
    snprintf(dst,cap,"%s",path?path:"");
    path_normalize(dst);
    slash=strrchr(dst,'/');
    if(slash) dst[(size_t)(slash-dst)]=0;
    else dst[0]=0;
}

static void path_stem_copy(char *dst,size_t cap,const char *path){
    char base[MAX_PATH_LEN];
    const char *dot;
    size_t n;
    if(!dst||cap==0)return;
    snprintf(base,sizeof(base),"%s",path_basename_ptr(path));
    dot=strrchr(base,'.');
    if(dot) base[(size_t)(dot-base)]=0;
    n=strlen(base);
    if(n>=cap)n=cap-1;
    memcpy(dst,base,n);
    dst[n]=0;
}

static bool file_exists(const char *path){
    struct stat st;
    return stat(path,&st)==0&&S_ISREG(st.st_mode);
}

static void mkdirs_for_file(const char *path){
    char tmp[MAX_PATH_LEN];
    snprintf(tmp,sizeof(tmp),"%s",path);
    path_normalize(tmp);
    for(char *p=tmp;*p;p++){
        if(*p!='/')continue;
        *p=0;
        if(tmp[0]) mkdir_one(tmp);
        *p='/';
    }
}

static void path_join(char *dst,size_t cap,const char *lhs,const char *rhs){
    size_t lhs_len,rhs_len,max_rhs;
    if(!dst||cap==0)return;
    lhs=lhs?lhs:"";
    rhs=rhs?rhs:"";
    lhs_len=strlen(lhs);
    rhs_len=strlen(rhs);
    if(lhs_len>=cap) lhs_len=cap-1;
    memcpy(dst,lhs,lhs_len);
    if(lhs_len+1<cap){
        dst[lhs_len]='/';
        max_rhs=cap-lhs_len-2;
        if(rhs_len>max_rhs) rhs_len=max_rhs;
        memcpy(dst+lhs_len+1,rhs,rhs_len);
        dst[lhs_len+1+rhs_len]=0;
    }else{
        dst[lhs_len]=0;
    }
    path_normalize(dst);
}

static void path_replace_ext(char *dst,size_t cap,const char *path,const char *ext){
    size_t n;
    const char *dot;
    if(!dst||cap==0)return;
    snprintf(dst,cap,"%s",path?path:"");
    dot=strrchr(dst,'.');
    if(dot){
        n=(size_t)(dot-dst);
        if(n<cap) dst[n]=0;
    }
    n=strlen(dst);
    if(n<cap-1&&ext){
        size_t ext_len=strlen(ext);
        if(ext_len>cap-n-1) ext_len=cap-n-1;
        memcpy(dst+n,ext,ext_len);
        dst[n+ext_len]=0;
    }
}

static void path_append_suffix(char *dst,size_t cap,const char *path,const char *suffix){
    size_t path_len,suffix_len,n;
    if(!dst||cap==0)return;
    path=path?path:"";
    suffix=suffix?suffix:"";
    path_len=strlen(path);
    suffix_len=strlen(suffix);
    n=path_len;
    if(n>=cap)n=cap-1;
    memcpy(dst,path,n);
    if(n<cap-1){
        size_t room=cap-n-1;
        if(suffix_len>room)suffix_len=room;
        memcpy(dst+n,suffix,suffix_len);
        n+=suffix_len;
    }
    dst[n]=0;
}

static void prune_model_output_variants(const char *base_path,const char *keep_path){
    static const char *exts[]={
        ".modelc",".modelc.meta",".fbbin",".fbbin.meta",".glbbin",".glbbin.meta"
    };
    char candidate[MAX_PATH_LEN];
    for(size_t i=0;i<sizeof(exts)/sizeof(exts[0]);i++){
        path_replace_ext(candidate,sizeof(candidate),base_path,exts[i]);
        path_normalize(candidate);
        if(keep_path&&strcmp(candidate,keep_path)==0)continue;
        unlink(candidate);
    }
}

static void asset_set_cooked_ext(CookedAsset *asset,const char *ext){
    char cooked[MAX_PATH_LEN];
    char source_base[MAX_PATH_LEN];
    if(!asset||!ext)return;
    path_replace_ext(source_base,sizeof(source_base),asset->cooked,"");
    path_replace_ext(cooked,sizeof(cooked),asset->cooked,ext);
    snprintf(asset->cooked,sizeof(asset->cooked),"%s",cooked);
    path_append_suffix(asset->meta,sizeof(asset->meta),asset->cooked,".meta");
    path_normalize(asset->cooked);
    path_normalize(asset->meta);
    prune_model_output_variants(source_base,asset->cooked);
    prune_model_output_variants(source_base,asset->meta);
}

static bool copy_file(const char *src,const char *dst){
    FILE *in=fopen(src,"rb");
    FILE *out;
    char buf[64*1024];
    size_t n;
    if(!in){
        fprintf(stderr,"[Cook] open %s failed: %s\n",src,strerror(errno));
        return false;
    }
    mkdirs_for_file(dst);
    out=fopen(dst,"wb");
    if(!out){
        fclose(in);
        fprintf(stderr,"[Cook] create %s failed: %s\n",dst,strerror(errno));
        return false;
    }
    while((n=fread(buf,1,sizeof(buf),in))>0){
        if(fwrite(buf,1,n,out)!=n){
            fclose(in);
            fclose(out);
            return false;
        }
    }
    fclose(in);
    fclose(out);
    return true;
}

static bool write_file_bytes(const char *dst,const void *data,size_t size){
    FILE *out;
    if(!dst||(!data&&size>0))return false;
    mkdirs_for_file(dst);
    out=fopen(dst,"wb");
    if(!out)return false;
    if(size>0&&fwrite(data,1,size,out)!=size){
        fclose(out);
        return false;
    }
    fclose(out);
    return true;
}

static bool read_file_bytes(const char *path,void **out_data,size_t *out_size){
    FILE *f;
    long len;
    void *data;
    if(!out_data||!out_size)return false;
    *out_data=NULL;
    *out_size=0;
    f=fopen(path,"rb");
    if(!f)return false;
    if(fseek(f,0,SEEK_END)!=0){ fclose(f); return false; }
    len=ftell(f);
    if(len<0){ fclose(f); return false; }
    if(fseek(f,0,SEEK_SET)!=0){ fclose(f); return false; }
    data=malloc((size_t)len);
    if(!data){ fclose(f); return false; }
    if(len>0&&fread(data,1,(size_t)len,f)!=(size_t)len){ free(data); fclose(f); return false; }
    fclose(f);
    *out_data=data;
    *out_size=(size_t)len;
    return true;
}

static bool write_cooked_model_bundle(const char *src,const char *dst){
    CookedFbxHeader header;
    FILE *out;
    void *data=NULL;
    size_t size=0;
    size_t source_len=strlen(src);
    if(!read_file_bytes(src,&data,&size)) return false;
    memset(&header,0,sizeof(header));
    header.magic=COOKED_FBX_MAGIC;
    header.version=1;
    header.source_path_len=(uint32_t)source_len;
    header.payload_size=(uint64_t)size;
    mkdirs_for_file(dst);
    out=fopen(dst,"wb");
    if(!out){ free(data); return false; }
    if(fwrite(&header,1,sizeof(header),out)!=sizeof(header)||
       fwrite(src,1,source_len,out)!=source_len||
       (size>0&&fwrite(data,1,size,out)!=size)){
        free(data);
        fclose(out);
        return false;
    }
    free(data);
    fclose(out);
    return true;
}

static uint64_t hash_file_fnv1a64(const char *path){
    FILE *f=fopen(path,"rb");
    uint64_t hash=1469598103934665603ULL;
    unsigned char buf[64*1024];
    size_t n;
    if(!f)return 0;
    while((n=fread(buf,1,sizeof(buf),f))>0){
        for(size_t i=0;i<n;i++){
            hash^=(uint64_t)buf[i];
            hash*=1099511628211ULL;
        }
    }
    fclose(f);
    return hash;
}

static void hash_to_hex(uint64_t hash,char out[17]){
    snprintf(out,17,"%016llx",(unsigned long long)hash);
}

static AssetKind classify_asset(const char *path){
    const char *dot=strrchr(path,'.');
    if(!dot)return ASSET_UNKNOWN;
    dot++;
    if(strcasecmp(dot,"fbx")==0||strcasecmp(dot,"gltf")==0||strcasecmp(dot,"glb")==0) return ASSET_MODEL;
    if(strcasecmp(dot,"png")==0||strcasecmp(dot,"jpg")==0||strcasecmp(dot,"jpeg")==0||
       strcasecmp(dot,"tga")==0||strcasecmp(dot,"bmp")==0) return ASSET_TEXTURE;
    if(strcasecmp(dot,"wav")==0||strcasecmp(dot,"ogg")==0||strcasecmp(dot,"mp3")==0) return ASSET_AUDIO;
    if(strcasecmp(dot,"fs")==0||strcasecmp(dot,"vs")==0||strcasecmp(dot,"vert")==0||
       strcasecmp(dot,"frag")==0||strcasecmp(dot,"glsl")==0) return ASSET_SHADER;
    if(strcasecmp(dot,"txt")==0||strcasecmp(dot,"json")==0||strcasecmp(dot,"csv")==0) return ASSET_TEXT;
    if(strcasecmp(dot,"bin")==0) return ASSET_DATA;
    return ASSET_UNKNOWN;
}

static bool is_cookable_file(const char *path){
    return classify_asset(path)!=ASSET_UNKNOWN;
}

static const char *asset_kind_name(AssetKind kind){
    switch(kind){
        case ASSET_MODEL: return "model";
        case ASSET_TEXTURE: return "texture";
        case ASSET_AUDIO: return "audio";
        case ASSET_SHADER: return "shader";
        case ASSET_DATA: return "data";
        case ASSET_TEXT: return "text";
        default: return "unknown";
    }
}

static int ctx_find_asset(CookContext *ctx,const char *source){
    for(int i=0;i<ctx->asset_count;i++){
        if(strcmp(ctx->assets[i].source,source)==0) return i;
    }
    return -1;
}

static int ctx_add_asset(CookContext *ctx,const char *source,AssetKind kind){
    CookedAsset *asset;
    size_t root_len,src_len,max_src;
    if(ctx->asset_count>=MAX_ASSETS) return -1;
    asset=&ctx->assets[ctx->asset_count];
    memset(asset,0,sizeof(*asset));
    snprintf(asset->source,sizeof(asset->source),"%s",source);
    path_normalize(asset->source);
    asset->kind=kind;
    path_join(asset->cooked,sizeof(asset->cooked),ctx->out_root,asset->source);
    if(kind==ASSET_MODEL&&strrchr(asset->source,'.')){
        const char *ext=strrchr(asset->source,'.');
        char cooked_bin[MAX_PATH_LEN];
        if(strcasecmp(ext,".fbx")==0){
            path_replace_ext(cooked_bin,sizeof(cooked_bin),asset->cooked,".fbbin");
            snprintf(asset->cooked,sizeof(asset->cooked),"%s",cooked_bin);
        }else if(strcasecmp(ext,".glb")==0){
            path_replace_ext(cooked_bin,sizeof(cooked_bin),asset->cooked,".glbbin");
            snprintf(asset->cooked,sizeof(asset->cooked),"%s",cooked_bin);
        }
    }
    path_normalize(asset->cooked);
    path_append_suffix(asset->meta,sizeof(asset->meta),asset->cooked,".meta");
    path_normalize(asset->meta);
    ctx->asset_count++;
    return ctx->asset_count-1;
}

static int ctx_get_or_add_asset(CookContext *ctx,const char *source,AssetKind kind){
    char normalized[MAX_PATH_LEN];
    int idx;
    snprintf(normalized,sizeof(normalized),"%s",source);
    path_normalize(normalized);
    idx=ctx_find_asset(ctx,normalized);
    if(idx>=0)return idx;
    return ctx_add_asset(ctx,normalized,kind);
}

static void asset_add_dep(CookedAsset *asset,int dep_idx){
    if(dep_idx<0)return;
    for(int i=0;i<asset->dep_count;i++) if(asset->deps[i]==dep_idx) return;
    if(asset->dep_count<MAX_DEPS_PER_ASSET) asset->deps[asset->dep_count++]=dep_idx;
}

static void write_meta_header(FILE *f,CookedAsset *asset){
    fprintf(f,"kind=%s\n",asset_kind_name(asset->kind));
    fprintf(f,"source=%s\n",asset->source);
    fprintf(f,"cooked=%s\n",asset->cooked);
    fprintf(f,"hash=%s\n",asset->hash);
}

static bool write_texture_meta(CookContext *ctx,CookedAsset *asset){
    int w=0,h=0,ch=0;
    FILE *f;
    if(!stbi_info(asset->source,&w,&h,&ch)){
        fprintf(stderr,"[Cook] invalid texture %s\n",asset->source);
        return false;
    }
    mkdirs_for_file(asset->meta);
    f=fopen(asset->meta,"w");
    if(!f)return false;
    (void)ctx;
    write_meta_header(f,asset);
    fprintf(f,"width=%d\nheight=%d\nchannels=%d\n",w,h,ch);
    fclose(f);
    return true;
}

static bool write_audio_meta(CookContext *ctx,CookedAsset *asset){
    FILE *f;
    struct stat st;
    (void)ctx;
    if(stat(asset->source,&st)!=0)return false;
    mkdirs_for_file(asset->meta);
    f=fopen(asset->meta,"w");
    if(!f)return false;
    write_meta_header(f,asset);
    fprintf(f,"bytes=%lld\n",(long long)st.st_size);
    fclose(f);
    return true;
}

static bool write_generic_meta(CookContext *ctx,CookedAsset *asset){
    FILE *f;
    struct stat st;
    (void)ctx;
    if(stat(asset->source,&st)!=0)return false;
    mkdirs_for_file(asset->meta);
    f=fopen(asset->meta,"w");
    if(!f)return false;
    write_meta_header(f,asset);
    fprintf(f,"bytes=%lld\n",(long long)st.st_size);
    fclose(f);
    return true;
}

static bool resolve_model_relative(char *dst,size_t cap,const char *model_path,const char *rel_path){
    char model_dir[MAX_PATH_LEN];
    char model_stem[256];
    char candidate[MAX_PATH_LEN];
    if(!dst||cap==0)return false;
    dst[0]=0;
    if(!rel_path||!rel_path[0])return false;

    snprintf(candidate,sizeof(candidate),"%s",rel_path);
    path_normalize(candidate);
    if(file_exists(candidate)){
        snprintf(dst,cap,"%s",candidate);
        return true;
    }

    path_dirname_copy(model_dir,sizeof(model_dir),model_path);
    path_stem_copy(model_stem,sizeof(model_stem),model_path);

    if(model_dir[0]){
        snprintf(candidate,sizeof(candidate),"%s/%s",model_dir,rel_path);
        path_normalize(candidate);
        if(file_exists(candidate)){
            snprintf(dst,cap,"%s",candidate);
            return true;
        }
        if(model_stem[0]){
            snprintf(candidate,sizeof(candidate),"%s/%s/%s",model_dir,model_stem,rel_path);
            path_normalize(candidate);
            if(file_exists(candidate)){
                snprintf(dst,cap,"%s",candidate);
                return true;
            }
        }
    }
    return false;
}

static void copy_norm(char *dst,size_t cap,const char *src){
    if(!dst||cap==0)return;
    snprintf(dst,cap,"%s",src?src:"");
    path_normalize(dst);
}

static bool write_map_path(char *dst,size_t cap,const char *model_path,ufbx_texture *tex){
    char rel[MAX_PATH_LEN]={0};
    if(!dst||cap==0){return false;}
    dst[0]=0;
    if(!tex)return false;
    if(tex->relative_filename.length){
        size_t n=tex->relative_filename.length<sizeof(rel)-1?tex->relative_filename.length:sizeof(rel)-1;
        memcpy(rel,tex->relative_filename.data,n);
        rel[n]=0;
        path_normalize(rel);
        if(resolve_model_relative(dst,cap,model_path,rel)) return true;
    }
    if(tex->filename.length){
        size_t n=tex->filename.length<sizeof(rel)-1?tex->filename.length:sizeof(rel)-1;
        memcpy(rel,tex->filename.data,n);
        rel[n]=0;
        path_normalize(rel);
        if(file_exists(rel)){ copy_norm(dst,cap,rel); return true; }
    }
    return false;
}

static bool write_material_map_path(char *dst,size_t cap,const char *model_path,const ufbx_material_map *map){
    if(!map||!map->texture_enabled||!map->texture)return false;
    return write_map_path(dst,cap,model_path,map->texture);
}

static float cook_uv_stream_score(const ufbx_mesh *mesh,const ufbx_vertex_vec2 *uvs){
    float umin=1e30f,vmin=1e30f,umax=-1e30f,vmax=-1e30f;
    if(!mesh||!uvs||!uvs->exists||mesh->num_indices==0)return -1.0f;
    for(size_t ix=0;ix<mesh->num_indices;ix++){
        ufbx_vec2 uv=ufbx_get_vertex_vec2(uvs,ix);
        if((float)uv.x<umin)umin=(float)uv.x;
        if((float)uv.y<vmin)vmin=(float)uv.y;
        if((float)uv.x>umax)umax=(float)uv.x;
        if((float)uv.y>vmax)vmax=(float)uv.y;
    }
    {
        float du=umax-umin,dv=vmax-vmin;
        if(du<1e-5f&&dv<1e-5f)return 0.0f;
        return du*dv;
    }
}

static bool path_contains_ci_local(const char *path,const char *needle){
    size_t nlen;
    if(!path||!needle||!*needle)return false;
    nlen=strlen(needle);
    for(const char *p=path;*p;p++){
        size_t i=0;
        while(i<nlen&&p[i]&&tolower((unsigned char)p[i])==tolower((unsigned char)needle[i]))i++;
        if(i==nlen)return true;
    }
    return false;
}

static const ufbx_uv_set *cook_mesh_preferred_uv_set(const ufbx_mesh *mesh){
    const ufbx_uv_set *best=NULL;
    float best_score=-1.0f;
    if(!mesh)return NULL;
    for(size_t i=0;i<mesh->uv_sets.count;i++){
        const ufbx_uv_set *uv=&mesh->uv_sets.data[i];
        float score=cook_uv_stream_score(mesh,&uv->vertex_uv);
        if(score<=0.0f)continue;
        if(path_contains_ci_local(uv->name.data,"UVMap"))score+=1000.0f;
        if(score>best_score){best=uv;best_score=score;}
    }
    return best;
}

static const ufbx_vertex_vec2 *cook_mesh_preferred_uv(const ufbx_mesh *mesh){
    const ufbx_uv_set *set;
    if(mesh&&mesh->vertex_uv.exists&&cook_uv_stream_score(mesh,&mesh->vertex_uv)>0.0f)return &mesh->vertex_uv;
    set=cook_mesh_preferred_uv_set(mesh);
    return set?&set->vertex_uv:NULL;
}

static const ufbx_vertex_vec3 *cook_mesh_preferred_tangent(const ufbx_mesh *mesh){
    const ufbx_uv_set *set;
    if(mesh&&mesh->vertex_tangent.exists&&cook_uv_stream_score(mesh,&mesh->vertex_uv)>0.0f)return &mesh->vertex_tangent;
    set=cook_mesh_preferred_uv_set(mesh);
    return (set&&set->vertex_tangent.exists)?&set->vertex_tangent:NULL;
}

static const ufbx_vertex_vec3 *cook_mesh_preferred_bitangent(const ufbx_mesh *mesh){
    const ufbx_uv_set *set;
    if(mesh&&mesh->vertex_bitangent.exists&&cook_uv_stream_score(mesh,&mesh->vertex_uv)>0.0f)return &mesh->vertex_bitangent;
    set=cook_mesh_preferred_uv_set(mesh);
    return (set&&set->vertex_bitangent.exists)?&set->vertex_bitangent:NULL;
}

static CookMeshNormals cook_mesh_normals_build(ufbx_mesh *mesh){
    CookMeshNormals mn={0};
    ufbx_topo_edge *topo;
    uint32_t *normal_indices;
    size_t num_normals;
    if(mesh->num_indices==0)return mn;
    topo=malloc(mesh->num_indices*sizeof(ufbx_topo_edge));
    normal_indices=malloc(mesh->num_indices*sizeof(uint32_t));
    if(!topo||!normal_indices){ free(topo); free(normal_indices); return mn; }
    ufbx_compute_topology(mesh,topo,mesh->num_indices);
    num_normals=ufbx_generate_normal_mapping(mesh,topo,mesh->num_indices,normal_indices,mesh->num_indices,false);
    free(topo);
    if(num_normals==0){ free(normal_indices); return mn; }
    mn.normals=malloc(num_normals*sizeof(ufbx_vec3));
    if(!mn.normals){ free(normal_indices); return mn; }
    ufbx_compute_normals(mesh,&mesh->vertex_position,normal_indices,mesh->num_indices,mn.normals,num_normals);
    mn.indices=normal_indices;
    mn.num_normals=num_normals;
    return mn;
}

static ufbx_vec3 cook_mesh_normal_get(const CookMeshNormals *mn,ufbx_mesh *mesh,uint32_t ix){
    if(mn->normals&&mn->indices&&ix<mesh->num_indices){
        uint32_t ni=mn->indices[ix];
        if(ni<mn->num_normals)return mn->normals[ni];
    }
    if(mesh->vertex_normal.exists)return ufbx_get_vertex_vec3(&mesh->vertex_normal,ix);
    return (ufbx_vec3){0.0,1.0,0.0};
}

static void cook_mesh_normals_free(CookMeshNormals *mn){
    free(mn->normals);
    free(mn->indices);
    memset(mn,0,sizeof(*mn));
}

static ufbx_material *cook_mesh_material_at(ufbx_mesh *mesh,ufbx_node *node,size_t index){
    if(node&&index<node->materials.count){
        ufbx_material *mat=node->materials.data[index];
        if(mat)return mat;
    }
    if(mesh&&index<mesh->materials.count)return mesh->materials.data[index];
    return NULL;
}

static void cook_mat4_from_ufbx(float out[16],const ufbx_matrix *u){
    out[0]=(float)u->m00; out[1]=(float)u->m10; out[2]=(float)u->m20; out[3]=0.0f;
    out[4]=(float)u->m01; out[5]=(float)u->m11; out[6]=(float)u->m21; out[7]=0.0f;
    out[8]=(float)u->m02; out[9]=(float)u->m12; out[10]=(float)u->m22; out[11]=0.0f;
    out[12]=(float)u->m03; out[13]=(float)u->m13; out[14]=(float)u->m23; out[15]=1.0f;
}

static void cook_mat4_from_cgltf(float out[16],const cgltf_float *m16){
    for(int i=0;i<16;i++)out[i]=(float)m16[i];
}

static double cook_anim_stack_real_end(ufbx_anim_stack *stk){
    double end=stk->time_begin;
    for(size_t li=0;li<stk->layers.count;li++){
        ufbx_anim_layer *layer=stk->layers.data[li];
        for(size_t vi=0;vi<layer->anim_values.count;vi++){
            ufbx_anim_value *av=layer->anim_values.data[vi];
            for(int k=0;k<3;k++){
                ufbx_anim_curve *curve=av->curves[k];
                if(!curve||curve->keyframes.count==0)continue;
                double kt=curve->keyframes.data[curve->keyframes.count-1].time;
                if(kt>end)end=kt;
            }
        }
    }
    if(end<=stk->time_begin)end=stk->time_end;
    return end;
}

static void manifest_write_asset(CookContext *ctx,CookedAsset *asset){
    fprintf(ctx->manifest,"%s|%s|%s|%s|",asset->source,asset->cooked,
            asset_kind_name(asset->kind),asset->hash);
    for(int i=0;i<asset->dep_count;i++){
        CookedAsset *dep=&ctx->assets[asset->deps[i]];
        if(i) fputc(';',ctx->manifest);
        fputs(dep->source,ctx->manifest);
    }
    fputc('\n',ctx->manifest);
}

static bool import_asset(CookContext *ctx,int asset_idx);

static bool import_dependency(CookContext *ctx,CookedAsset *owner,const char *dep_path){
    AssetKind kind;
    int dep_idx;
    char normalized[MAX_PATH_LEN];
    if(!dep_path||!dep_path[0]||!file_exists(dep_path)) return false;
    kind=classify_asset(dep_path);
    if(kind==ASSET_UNKNOWN) return false;
    snprintf(normalized,sizeof(normalized),"%s",dep_path);
    path_normalize(normalized);
    dep_idx=ctx_get_or_add_asset(ctx,normalized,kind);
    if(dep_idx<0)return false;
    asset_add_dep(owner,dep_idx);
    return import_asset(ctx,dep_idx);
}

static bool gather_fbx_dependencies(CookContext *ctx,CookedAsset *asset){
    ufbx_load_opts opts;
    ufbx_error err;
    ufbx_scene *scene;
    memset(&opts,0,sizeof(opts));
    scene=ufbx_load_file(asset->source,&opts,&err);
    if(!scene){
        fprintf(stderr,"[Cook] FBX parse failed for %s\n",asset->source);
        return false;
    }
    for(size_t i=0;i<scene->textures.count;i++){
        ufbx_texture *tex=scene->textures.data[i];
        char rel[MAX_PATH_LEN]={0};
        char abs_path[MAX_PATH_LEN]={0};
        if(tex->relative_filename.length){
            size_t n=tex->relative_filename.length<sizeof(rel)-1?tex->relative_filename.length:sizeof(rel)-1;
            memcpy(rel,tex->relative_filename.data,n);
            rel[n]=0;
            path_normalize(rel);
            if(resolve_model_relative(abs_path,sizeof(abs_path),asset->source,rel))
                import_dependency(ctx,asset,abs_path);
        }
        if(tex->filename.length){
            size_t n=tex->filename.length<sizeof(rel)-1?tex->filename.length:sizeof(rel)-1;
            memcpy(rel,tex->filename.data,n);
            rel[n]=0;
            path_normalize(rel);
            if(file_exists(rel)) import_dependency(ctx,asset,rel);
        }
    }
    ufbx_free_scene(scene);
    return true;
}

static bool fbx_scene_is_static(ufbx_scene *scene){
    if(!scene)return false;
    if(scene->anim_stacks.count>0)return false;
    for(size_t ni=0;ni<scene->nodes.count;ni++){
        ufbx_node *nd=scene->nodes.data[ni];
        if(nd->mesh&&nd->mesh->skin_deformers.count>0)return false;
    }
    return true;
}

static bool write_texture_dep(CookContext *ctx,CookedAsset *asset,char *dst,size_t cap,const char *path){
    if(path&&path[0]){
        copy_norm(dst,cap,path);
        return import_dependency(ctx,asset,path);
    }
    if(dst&&cap>0)dst[0]=0;
    return false;
}

static int fbx_skin_index(ufbx_scene *scene,ufbx_skin_deformer *skin){
    if(!scene||!skin)return -1;
    for(size_t i=0;i<scene->skin_deformers.count;i++) if(scene->skin_deformers.data[i]==skin) return (int)i;
    return -1;
}

static bool export_static_fbx_modelc(CookContext *ctx,CookedAsset *asset){
    ufbx_load_opts opts={0};
    ufbx_error err;
    ufbx_scene *scene;
    FILE *out;
    ModelcHeader header;
    bool is_static;
    uint32_t mesh_count=0;
    float xmin=1e9f,ymin=1e9f,zmin=1e9f,xmax=-1e9f,ymax=-1e9f,zmax=-1e9f;
    float ymin_local=1e9f,ymax_local=-1e9f;

    opts.generate_missing_normals=true;
    opts.target_axes=ufbx_axes_right_handed_y_up;
    opts.target_unit_meters=1.0f;
    opts.use_blender_pbr_material=true;
    opts.normalize_tangents=true;
    scene=ufbx_load_file(asset->source,&opts,&err);
    if(!scene){
        fprintf(stderr,"[Cook] FBX parse failed for %s\n",asset->source);
        return false;
    }
    is_static=fbx_scene_is_static(scene);

    asset_set_cooked_ext(asset,".modelc");
    mkdirs_for_file(asset->cooked);
    out=fopen(asset->cooked,"wb");
    if(!out){ ufbx_free_scene(scene); return false; }

    memset(&header,0,sizeof(header));
    header.magic=MODELC_MAGIC;
    header.version=is_static?MODELC_VERSION:MODELC_VERSION_ANIM;

    if(fwrite(&header,1,sizeof(header),out)!=sizeof(header)){ fclose(out); ufbx_free_scene(scene); return false; }

    for(size_t ni=0;ni<scene->nodes.count;ni++){
        ufbx_node *nd=scene->nodes.data[ni];
        if(!nd->mesh)continue;
        size_t mat_count=nd->mesh->materials.count;
        bool split_by_material=mat_count>1&&nd->mesh->face_material.count==nd->mesh->num_faces;
        size_t passes=split_by_material?mat_count:1;
        for(size_t part=0;part<passes;part++){
            ufbx_mesh *mesh=nd->mesh;
            const ufbx_vertex_vec2 *uv_stream=cook_mesh_preferred_uv(mesh);
            const ufbx_vertex_vec3 *tan_stream=cook_mesh_preferred_tangent(mesh);
            const ufbx_vertex_vec3 *bitan_stream=cook_mesh_preferred_bitangent(mesh);
            CookMeshNormals mesh_normals=cook_mesh_normals_build(mesh);
            size_t ntri=mesh->max_face_triangles*3;
            uint32_t *tri=malloc(ntri*sizeof(uint32_t));
            ModelcVertex *verts=NULL;
            uint32_t *inds=NULL;
            size_t vc=0,ic=0,vcap=4096,icap=4096;
            ModelcMeshHeader mh;
            ufbx_material *mat;
            if(!tri){ cook_mesh_normals_free(&mesh_normals); fclose(out); ufbx_free_scene(scene); return false; }
            verts=malloc(vcap*sizeof(ModelcVertex));
            inds=malloc(icap*sizeof(uint32_t));
            if(!verts||!inds){
                free(tri); free(verts); free(inds); cook_mesh_normals_free(&mesh_normals); fclose(out); ufbx_free_scene(scene); return false;
            }
            for(size_t fi=0;fi<mesh->num_faces;fi++){
                if(split_by_material){
                    uint32_t face_mat=mesh->face_material.data[fi];
                    if(face_mat!=part)continue;
                }
                {
                    ufbx_face face=mesh->faces.data[fi];
                    size_t nt=ufbx_triangulate_face(tri,ntri,mesh,face);
                    for(size_t ti=0;ti<nt*3;ti++){
                        uint32_t ix=tri[ti];
                        ModelcVertex v={0};
                        ufbx_vec3 p=ufbx_get_vertex_vec3(&mesh->vertex_position,ix);
                        ufbx_vec3 n=cook_mesh_normal_get(&mesh_normals,mesh,ix);
                        v.pos[0]=(float)p.x; v.pos[1]=(float)p.y; v.pos[2]=(float)p.z;
                        v.normal[0]=(float)n.x; v.normal[1]=(float)n.y; v.normal[2]=(float)n.z;
                        if(uv_stream&&uv_stream->exists){
                            ufbx_vec2 uv=ufbx_get_vertex_vec2(uv_stream,ix);
                            v.uv[0]=(float)uv.x; v.uv[1]=(float)uv.y;
                        }
                        if(tan_stream&&tan_stream->exists){
                            ufbx_vec3 t=ufbx_get_vertex_vec3(tan_stream,ix);
                            v.tangent[0]=(float)t.x; v.tangent[1]=(float)t.y; v.tangent[2]=(float)t.z;
                        }
                        if(bitan_stream&&bitan_stream->exists){
                            ufbx_vec3 b=ufbx_get_vertex_vec3(bitan_stream,ix);
                            v.bitangent[0]=(float)b.x; v.bitangent[1]=(float)b.y; v.bitangent[2]=(float)b.z;
                        }else{
                            float nx=v.normal[0],ny=v.normal[1],nz=v.normal[2];
                            float tx=v.tangent[0],ty=v.tangent[1],tz=v.tangent[2];
                            float tlen=fabsf(tx)+fabsf(ty)+fabsf(tz);
                            if(tlen<1e-5f){
                                float ax=fabsf(ny)<0.99f?0.0f:1.0f;
                                float ay=fabsf(ny)<0.99f?1.0f:0.0f;
                                float az=0.0f;
                                float dot= ax*nx+ay*ny+az*nz;
                                tx=ax-nx*dot; ty=ay-ny*dot; tz=az-nz*dot;
                                {
                                    float len=sqrtf(tx*tx+ty*ty+tz*tz);
                                    if(len>1e-5f){ tx/=len; ty/=len; tz/=len; }
                                }
                                v.tangent[0]=tx; v.tangent[1]=ty; v.tangent[2]=tz;
                            }
                            {
                                float bx=ny*tz-nz*ty;
                                float by=nz*tx-nx*tz;
                                float bz=nx*ty-ny*tx;
                                float len=sqrtf(bx*bx+by*by+bz*bz);
                                if(len>1e-5f){ bx/=len; by/=len; bz/=len; }
                                v.bitangent[0]=bx; v.bitangent[1]=by; v.bitangent[2]=bz;
                            }
                        }
                        if(vc>=vcap){ vcap*=2; verts=realloc(verts,vcap*sizeof(ModelcVertex)); }
                        if(ic>=icap){ icap*=2; inds=realloc(inds,icap*sizeof(uint32_t)); }
                        if(!verts||!inds){
                            free(tri); free(verts); free(inds); cook_mesh_normals_free(&mesh_normals); fclose(out); ufbx_free_scene(scene); return false;
                        }
                        inds[ic++]=(uint32_t)vc;
                        verts[vc++]=v;
                    }
                }
            }
            free(tri);
            cook_mesh_normals_free(&mesh_normals);
            if(vc==0||ic==0){ free(verts); free(inds); continue; }

            if(is_static){
                memset(&mh,0,sizeof(mh));
                mh.vertex_count=(uint32_t)vc;
                mh.index_count=(uint32_t)ic;
                cook_mat4_from_ufbx(mh.geometry_to_world,&nd->geometry_to_world);
                mh.node_typed_id=nd->typed_id;
                mh.mat_color[0]=0.75f; mh.mat_color[1]=0.72f; mh.mat_color[2]=0.68f; mh.mat_color[3]=1.0f;
                mh.roughness=0.85f;
                mh.alpha_cutoff=0.33f;
                mat=cook_mesh_material_at(mesh,nd,split_by_material?part:0);
                if(mat){
                    char tex_path[MAX_PATH_LEN]={0};
                    char normal_path[MAX_PATH_LEN]={0};
                    char mr_path[MAX_PATH_LEN]={0};
                    char emissive_path[MAX_PATH_LEN]={0};
                    ufbx_texture *base_tex=mat->pbr.base_color.texture?mat->pbr.base_color.texture:mat->fbx.diffuse_color.texture;
                    if(base_tex&&write_map_path(tex_path,sizeof(tex_path),asset->source,base_tex))
                        write_texture_dep(ctx,asset,mh.texture_path,sizeof(mh.texture_path),tex_path);
                    if(write_material_map_path(normal_path,sizeof(normal_path),asset->source,&mat->pbr.normal_map))
                        write_texture_dep(ctx,asset,mh.normal_path,sizeof(mh.normal_path),normal_path);
                    if(write_material_map_path(mr_path,sizeof(mr_path),asset->source,&mat->pbr.roughness)||
                       write_material_map_path(mr_path,sizeof(mr_path),asset->source,&mat->pbr.metalness))
                        write_texture_dep(ctx,asset,mh.mr_path,sizeof(mh.mr_path),mr_path);
                    if(write_material_map_path(emissive_path,sizeof(emissive_path),asset->source,&mat->pbr.emission_color))
                        write_texture_dep(ctx,asset,mh.emissive_path,sizeof(mh.emissive_path),emissive_path);
                    if(mat->pbr.base_color.value_components>=4){
                        mh.mat_color[0]=(float)mat->pbr.base_color.value_vec4.x;
                        mh.mat_color[1]=(float)mat->pbr.base_color.value_vec4.y;
                        mh.mat_color[2]=(float)mat->pbr.base_color.value_vec4.z;
                        mh.mat_color[3]=(float)mat->pbr.base_color.value_vec4.w;
                    }
                    if(mat->pbr.emission_color.value_components>=3){
                        mh.emissive[0]=(float)mat->pbr.emission_color.value_vec3.x;
                        mh.emissive[1]=(float)mat->pbr.emission_color.value_vec3.y;
                        mh.emissive[2]=(float)mat->pbr.emission_color.value_vec3.z;
                    }
                    if(mat->pbr.metalness.has_value) mh.metallic=(float)mat->pbr.metalness.value_real;
                    if(mat->pbr.roughness.has_value) mh.roughness=(float)mat->pbr.roughness.value_real;
                    if(mat->pbr.opacity.has_value) mh.mat_color[3]=(float)mat->pbr.opacity.value_real;
                    mh.has_pbr=(mh.texture_path[0]||mh.normal_path[0]||mh.mr_path[0]||mh.emissive_path[0]);
                }
                if(fwrite(&mh,1,sizeof(mh),out)!=sizeof(mh)||
                   fwrite(verts,sizeof(ModelcVertex),vc,out)!=vc||
                   fwrite(inds,sizeof(uint32_t),ic,out)!=ic){
                    free(verts); free(inds); fclose(out); ufbx_free_scene(scene); return false;
                }
            }else{
                ModelcMeshHeaderV2 mh2;
                memset(&mh2,0,sizeof(mh2));
                mh2.vertex_count=(uint32_t)vc;
                mh2.index_count=(uint32_t)ic;
                cook_mat4_from_ufbx(mh2.geometry_to_world,&nd->geometry_to_world);
                mh2.node_typed_id=nd->typed_id;
                mh2.skin_index=fbx_skin_index(scene,mesh->skin_deformers.count>0?mesh->skin_deformers.data[0]:NULL);
                mh2.num_gpu_bones=mesh->skin_deformers.count>0?(int32_t)(mesh->skin_deformers.data[0]->clusters.count<MAX_BONES?mesh->skin_deformers.data[0]->clusters.count:MAX_BONES):0;
                mh2.mat_color[0]=0.75f; mh2.mat_color[1]=0.72f; mh2.mat_color[2]=0.68f; mh2.mat_color[3]=1.0f;
                mh2.roughness=0.85f;
                mh2.alpha_cutoff=0.33f;
                mat=cook_mesh_material_at(mesh,nd,split_by_material?part:0);
                if(mat){
                    char tex_path[MAX_PATH_LEN]={0};
                    char normal_path[MAX_PATH_LEN]={0};
                    char mr_path[MAX_PATH_LEN]={0};
                    char emissive_path[MAX_PATH_LEN]={0};
                    ufbx_texture *base_tex=mat->pbr.base_color.texture?mat->pbr.base_color.texture:mat->fbx.diffuse_color.texture;
                    if(base_tex&&write_map_path(tex_path,sizeof(tex_path),asset->source,base_tex))
                        write_texture_dep(ctx,asset,mh2.texture_path,sizeof(mh2.texture_path),tex_path);
                    if(write_material_map_path(normal_path,sizeof(normal_path),asset->source,&mat->pbr.normal_map))
                        write_texture_dep(ctx,asset,mh2.normal_path,sizeof(mh2.normal_path),normal_path);
                    if(write_material_map_path(mr_path,sizeof(mr_path),asset->source,&mat->pbr.roughness)||
                       write_material_map_path(mr_path,sizeof(mr_path),asset->source,&mat->pbr.metalness))
                        write_texture_dep(ctx,asset,mh2.mr_path,sizeof(mh2.mr_path),mr_path);
                    if(write_material_map_path(emissive_path,sizeof(emissive_path),asset->source,&mat->pbr.emission_color))
                        write_texture_dep(ctx,asset,mh2.emissive_path,sizeof(mh2.emissive_path),emissive_path);
                    if(mat->pbr.base_color.value_components>=4){
                        mh2.mat_color[0]=(float)mat->pbr.base_color.value_vec4.x;
                        mh2.mat_color[1]=(float)mat->pbr.base_color.value_vec4.y;
                        mh2.mat_color[2]=(float)mat->pbr.base_color.value_vec4.z;
                        mh2.mat_color[3]=(float)mat->pbr.base_color.value_vec4.w;
                    }
                    if(mat->pbr.emission_color.value_components>=3){
                        mh2.emissive[0]=(float)mat->pbr.emission_color.value_vec3.x;
                        mh2.emissive[1]=(float)mat->pbr.emission_color.value_vec3.y;
                        mh2.emissive[2]=(float)mat->pbr.emission_color.value_vec3.z;
                    }
                    if(mat->pbr.metalness.has_value) mh2.metallic=(float)mat->pbr.metalness.value_real;
                    if(mat->pbr.roughness.has_value) mh2.roughness=(float)mat->pbr.roughness.value_real;
                    if(mat->pbr.opacity.has_value) mh2.mat_color[3]=(float)mat->pbr.opacity.value_real;
                    mh2.has_pbr=(mh2.texture_path[0]||mh2.normal_path[0]||mh2.mr_path[0]||mh2.emissive_path[0]);
                }
                if(fwrite(&mh2,1,sizeof(mh2),out)!=sizeof(mh2)||
                   fwrite(verts,sizeof(ModelcVertex),vc,out)!=vc||
                   fwrite(inds,sizeof(uint32_t),ic,out)!=ic){
                    free(verts); free(inds); fclose(out); ufbx_free_scene(scene); return false;
                }
            }

            for(size_t vi=0;vi<mesh->num_vertices;vi++){
                ufbx_vec3 lp=mesh->vertices.data[vi];
                ufbx_vec3 wp=ufbx_transform_position(&nd->geometry_to_world,lp);
                float x=(float)wp.x,y=(float)wp.y,z=(float)wp.z;
                if(x<xmin)xmin=x; if(x>xmax)xmax=x;
                if(y<ymin)ymin=y; if(y>ymax)ymax=y;
                if(z<zmin)zmin=z; if(z>zmax)zmax=z;
            }
            for(size_t fi=0;fi<mesh->num_faces;fi++){
                ufbx_face face=mesh->faces.data[fi];
                for(size_t i=face.index_begin;i<face.index_begin+face.num_indices;i++){
                    float y=(float)ufbx_get_vertex_vec3(&mesh->vertex_position,i).y;
                    if(y<ymin_local)ymin_local=y;
                    if(y>ymax_local)ymax_local=y;
                }
            }
            mesh_count++;
            free(verts);
            free(inds);
        }
    }

    header.mesh_count=mesh_count;
    header.ymin_raw=ymin;
    header.ymax_raw=ymax;
    header.cx=(xmin+xmax)*0.5f;
    header.cy=(ymin+ymax)*0.5f;
    header.cz=(zmin+zmax)*0.5f;
    {
        float md=fmaxf(xmax-xmin,fmaxf(ymax-ymin,zmax-zmin));
        if(md<0.0001f)md=1.0f;
        header.model_scale=2.0f/md;
        header.ground_offset=-(ymin-header.cy)*header.model_scale;
    }
    header.ymin_raw=ymin_local;
    header.ymax_raw=ymax_local;
    rewind(out);
    fwrite(&header,1,sizeof(header),out);
    if(!is_static){
        ModelcRigHeader rig={0};
        rig.node_count=(uint32_t)scene->nodes.count;
        rig.skin_count=(uint32_t)scene->skin_deformers.count;
        rig.anim_count=(uint32_t)scene->anim_stacks.count;
        fseek(out,0,SEEK_END);
        if(fwrite(&rig,1,sizeof(rig),out)!=sizeof(rig)){ fclose(out); ufbx_free_scene(scene); return false; }
        for(size_t ni=0;ni<scene->nodes.count;ni++){
            ufbx_node *nd=scene->nodes.data[ni];
            ModelcNodeRecord rec;
            memset(&rec,0,sizeof(rec));
            rec.parent_index=nd->parent?(int32_t)nd->parent->typed_id:-1;
            rec.t[0]=(float)nd->local_transform.translation.x;
            rec.t[1]=(float)nd->local_transform.translation.y;
            rec.t[2]=(float)nd->local_transform.translation.z;
            rec.s[0]=(float)nd->local_transform.scale.x;
            rec.s[1]=(float)nd->local_transform.scale.y;
            rec.s[2]=(float)nd->local_transform.scale.z;
            rec.r[0]=(float)nd->local_transform.rotation.x;
            rec.r[1]=(float)nd->local_transform.rotation.y;
            rec.r[2]=(float)nd->local_transform.rotation.z;
            rec.r[3]=(float)nd->local_transform.rotation.w;
            cook_mat4_from_ufbx(rec.rest_world,&nd->node_to_world);
            if(fwrite(&rec,1,sizeof(rec),out)!=sizeof(rec)){ fclose(out); ufbx_free_scene(scene); return false; }
        }
        for(size_t si=0;si<scene->skin_deformers.count;si++){
            ufbx_skin_deformer *skin=scene->skin_deformers.data[si];
            ModelcSkinRecord rec;
            memset(&rec,0,sizeof(rec));
            rec.joint_count=(int32_t)(skin->clusters.count<MAX_BONES?skin->clusters.count:MAX_BONES);
            for(int ji=0;ji<rec.joint_count;ji++){
                ufbx_skin_cluster *cl=skin->clusters.data[ji];
                rec.joint_nodes[ji]=cl->bone_node?(int32_t)cl->bone_node->typed_id:-1;
                cook_mat4_from_ufbx(rec.inverse_bind[ji],&cl->geometry_to_bone);
            }
            if(fwrite(&rec,1,sizeof(rec),out)!=sizeof(rec)){ fclose(out); ufbx_free_scene(scene); return false; }
        }
        for(size_t ai=0;ai<scene->anim_stacks.count;ai++){
            ufbx_anim_stack *stk=scene->anim_stacks.data[ai];
            ufbx_bake_opts bopts={0};
            ufbx_baked_anim *bake=ufbx_bake_anim(scene,stk->anim,&bopts,&err);
            ModelcAnimRecord arec;
            uint32_t channel_count=0;
            if(!bake){ fclose(out); ufbx_free_scene(scene); return false; }
            for(size_t bi=0;bi<bake->nodes.count;bi++){
                ufbx_baked_node *bn=&bake->nodes.data[bi];
                if(bn->translation_keys.count>0)channel_count++;
                if(bn->rotation_keys.count>0)channel_count++;
                if(bn->scale_keys.count>0)channel_count++;
            }
            memset(&arec,0,sizeof(arec));
            if(stk->name.data&&stk->name.length){
                size_t n=stk->name.length<sizeof(arec.name)-1?stk->name.length:sizeof(arec.name)-1;
                memcpy(arec.name,stk->name.data,n);
                arec.name[n]=0;
            }
            arec.duration=(float)(cook_anim_stack_real_end(stk)-stk->time_begin);
            arec.channel_count=channel_count;
            if(fwrite(&arec,1,sizeof(arec),out)!=sizeof(arec)){ ufbx_free_baked_anim(bake); fclose(out); ufbx_free_scene(scene); return false; }
            for(size_t bi=0;bi<bake->nodes.count;bi++){
                ufbx_baked_node *bn=&bake->nodes.data[bi];
                if(bn->translation_keys.count>0){
                    ModelcAnimChannelRecord crec={bn->typed_id,0u,0u,(uint32_t)bn->translation_keys.count};
                    float *times=malloc((size_t)crec.key_count*sizeof(*times));
                    float (*values)[4]=malloc((size_t)crec.key_count*sizeof(*values));
                    if((crec.key_count&&!times)||(crec.key_count&&!values)){ free(times); free(values); ufbx_free_baked_anim(bake); fclose(out); ufbx_free_scene(scene); return false; }
                    for(size_t ki=0;ki<bn->translation_keys.count;ki++){
                        times[ki]=(float)(bn->translation_keys.data[ki].time-stk->time_begin);
                        values[ki][0]=(float)bn->translation_keys.data[ki].value.x;
                        values[ki][1]=(float)bn->translation_keys.data[ki].value.y;
                        values[ki][2]=(float)bn->translation_keys.data[ki].value.z;
                        values[ki][3]=1.0f;
                    }
                    if(fwrite(&crec,1,sizeof(crec),out)!=sizeof(crec)){ ufbx_free_baked_anim(bake); fclose(out); ufbx_free_scene(scene); return false; }
                    if((crec.key_count&&fwrite(times,sizeof(*times),crec.key_count,out)!=crec.key_count)||
                       (crec.key_count&&fwrite(values,sizeof(*values),crec.key_count,out)!=crec.key_count)){ free(times); free(values); ufbx_free_baked_anim(bake); fclose(out); ufbx_free_scene(scene); return false; }
                    free(times); free(values);
                }
                if(bn->rotation_keys.count>0){
                    ModelcAnimChannelRecord crec={bn->typed_id,1u,0u,(uint32_t)bn->rotation_keys.count};
                    float *times=malloc((size_t)crec.key_count*sizeof(*times));
                    float (*values)[4]=malloc((size_t)crec.key_count*sizeof(*values));
                    if((crec.key_count&&!times)||(crec.key_count&&!values)){ free(times); free(values); ufbx_free_baked_anim(bake); fclose(out); ufbx_free_scene(scene); return false; }
                    for(size_t ki=0;ki<bn->rotation_keys.count;ki++){
                        times[ki]=(float)(bn->rotation_keys.data[ki].time-stk->time_begin);
                        values[ki][0]=(float)bn->rotation_keys.data[ki].value.x;
                        values[ki][1]=(float)bn->rotation_keys.data[ki].value.y;
                        values[ki][2]=(float)bn->rotation_keys.data[ki].value.z;
                        values[ki][3]=(float)bn->rotation_keys.data[ki].value.w;
                    }
                    if(fwrite(&crec,1,sizeof(crec),out)!=sizeof(crec)){ ufbx_free_baked_anim(bake); fclose(out); ufbx_free_scene(scene); return false; }
                    if((crec.key_count&&fwrite(times,sizeof(*times),crec.key_count,out)!=crec.key_count)||
                       (crec.key_count&&fwrite(values,sizeof(*values),crec.key_count,out)!=crec.key_count)){ free(times); free(values); ufbx_free_baked_anim(bake); fclose(out); ufbx_free_scene(scene); return false; }
                    free(times); free(values);
                }
                if(bn->scale_keys.count>0){
                    ModelcAnimChannelRecord crec={bn->typed_id,2u,0u,(uint32_t)bn->scale_keys.count};
                    float *times=malloc((size_t)crec.key_count*sizeof(*times));
                    float (*values)[4]=malloc((size_t)crec.key_count*sizeof(*values));
                    if((crec.key_count&&!times)||(crec.key_count&&!values)){ free(times); free(values); ufbx_free_baked_anim(bake); fclose(out); ufbx_free_scene(scene); return false; }
                    for(size_t ki=0;ki<bn->scale_keys.count;ki++){
                        times[ki]=(float)(bn->scale_keys.data[ki].time-stk->time_begin);
                        values[ki][0]=(float)bn->scale_keys.data[ki].value.x;
                        values[ki][1]=(float)bn->scale_keys.data[ki].value.y;
                        values[ki][2]=(float)bn->scale_keys.data[ki].value.z;
                        values[ki][3]=1.0f;
                    }
                    if(fwrite(&crec,1,sizeof(crec),out)!=sizeof(crec)){ ufbx_free_baked_anim(bake); fclose(out); ufbx_free_scene(scene); return false; }
                    if((crec.key_count&&fwrite(times,sizeof(*times),crec.key_count,out)!=crec.key_count)||
                       (crec.key_count&&fwrite(values,sizeof(*values),crec.key_count,out)!=crec.key_count)){ free(times); free(values); ufbx_free_baked_anim(bake); fclose(out); ufbx_free_scene(scene); return false; }
                    free(times); free(values);
                }
            }
            ufbx_free_baked_anim(bake);
        }
    }
    fclose(out);
    ufbx_free_scene(scene);
    return mesh_count>0;
}

static bool gltf_scene_is_static(const cgltf_data *data){
    if(!data)return false;
    if(data->animations_count>0||data->skins_count>0)return false;
    for(size_t ni=0;ni<data->nodes_count;ni++){
        const cgltf_node *node=&data->nodes[ni];
        if(node->skin)return false;
        if(!node->mesh)continue;
        for(size_t pi=0;pi<node->mesh->primitives_count;pi++){
            const cgltf_primitive *prim=&node->mesh->primitives[pi];
            for(size_t ai=0;ai<prim->attributes_count;ai++){
                cgltf_attribute_type type=prim->attributes[ai].type;
                if(type==cgltf_attribute_type_joints||type==cgltf_attribute_type_weights)return false;
            }
        }
    }
    return true;
}

static float cook_gltf_anim_duration(const cgltf_animation *anim){
    float end=0.0f;
    if(!anim)return 0.0f;
    for(size_t ci=0;ci<anim->channels_count;ci++){
        cgltf_animation_sampler *sam=anim->channels[ci].sampler;
        if(!sam||!sam->input||sam->input->count==0)continue;
        cgltf_float v=0.0f;
        cgltf_accessor_read_float(sam->input,sam->input->count-1,&v,1);
        if((float)v>end)end=(float)v;
    }
    return end;
}

static const char *gltf_image_ext(const cgltf_image *image){
    const char *uri;
    const char *dot;
    if(!image)return ".bin";
    uri=image->uri;
    if(uri&&uri[0]&&strncmp(uri,"data:",5)!=0){
        dot=strrchr(uri,'.');
        if(dot&&dot[1])return dot;
    }
    if(image->mime_type){
        if(strstr(image->mime_type,"png"))return ".png";
        if(strstr(image->mime_type,"jpeg")||strstr(image->mime_type,"jpg"))return ".jpg";
        if(strstr(image->mime_type,"bmp"))return ".bmp";
        if(strstr(image->mime_type,"tga"))return ".tga";
        if(strstr(image->mime_type,"webp"))return ".webp";
    }
    return ".bin";
}

static size_t gltf_image_index(const cgltf_data *data,const cgltf_image *image){
    if(!data||!image)return 0;
    for(size_t i=0;i<data->images_count;i++) if(&data->images[i]==image) return i;
    return 0;
}

static bool gltf_extract_embedded_image(char *dst,size_t cap,CookedAsset *asset,size_t image_index,const cgltf_image *image){
    char base[MAX_PATH_LEN];
    char path[MAX_PATH_LEN];
    char suffix[64];
    const void *data=NULL;
    size_t size=0;
    if(!dst||cap==0||!asset||!image)return false;
    dst[0]=0;
    if(image->buffer_view&&image->buffer_view->buffer&&image->buffer_view->buffer->data){
        data=cgltf_buffer_view_data(image->buffer_view);
        size=(size_t)image->buffer_view->size;
    }
    if(!data||size==0)return false;
    path_replace_ext(base,sizeof(base),asset->cooked,"");
    snprintf(suffix,sizeof(suffix),"__img%llu%s",(unsigned long long)image_index,gltf_image_ext(image));
    path_append_suffix(path,sizeof(path),base,suffix);
    path_normalize(path);
    if(!write_file_bytes(path,data,size)) return false;
    copy_norm(dst,cap,path);
    return true;
}

static bool gltf_texture_source_path(CookContext *ctx,CookedAsset *asset,const cgltf_data *data,const cgltf_texture_view *view,char *dst,size_t cap){
    const cgltf_image *image;
    char abs_path[MAX_PATH_LEN];
    if(!dst||cap==0)return false;
    dst[0]=0;
    if(!view||!view->texture)return false;
    image=view->texture->image?view->texture->image:(view->texture->has_basisu?view->texture->basisu_image:(view->texture->has_webp?view->texture->webp_image:NULL));
    if(!image)return false;
    if(image->uri&&image->uri[0]&&strncmp(image->uri,"data:",5)!=0){
        if(resolve_model_relative(abs_path,sizeof(abs_path),asset->source,image->uri)){
            if(import_dependency(ctx,asset,abs_path)){
                copy_norm(dst,cap,abs_path);
                return true;
            }
        }
    }
    if(gltf_extract_embedded_image(dst,cap,asset,gltf_image_index(data,image),image))return true;
    return false;
}

static bool export_static_gltf_modelc(CookContext *ctx,CookedAsset *asset){
    cgltf_options opts={0};
    cgltf_data *data=NULL;
    cgltf_result res;
    FILE *out;
    ModelcHeader header;
    bool is_static;
    uint32_t mesh_count=0;
    float xmin=1e9f,ymin=1e9f,zmin=1e9f,xmax=-1e9f,ymax=-1e9f,zmax=-1e9f;
    float ymin_local=1e9f,ymax_local=-1e9f;

    res=cgltf_parse_file(&opts,asset->source,&data);
    if(res!=cgltf_result_success||!data)return false;
    res=cgltf_load_buffers(&opts,data,asset->source);
    if(res!=cgltf_result_success){ cgltf_free(data); return false; }
    is_static=gltf_scene_is_static(data);

    asset_set_cooked_ext(asset,".modelc");
    mkdirs_for_file(asset->cooked);
    out=fopen(asset->cooked,"wb");
    if(!out){ cgltf_free(data); return false; }

    memset(&header,0,sizeof(header));
    header.magic=MODELC_MAGIC;
    header.version=is_static?MODELC_VERSION:MODELC_VERSION_ANIM;
    if(fwrite(&header,1,sizeof(header),out)!=sizeof(header)){ fclose(out); cgltf_free(data); return false; }

    for(size_t ni=0;ni<data->nodes_count;ni++){
        cgltf_node *node=&data->nodes[ni];
        cgltf_float world[16];
        if(!node->mesh)continue;
        cgltf_node_transform_world(node,world);
        for(size_t pi=0;pi<node->mesh->primitives_count;pi++){
            const cgltf_primitive *prim=&node->mesh->primitives[pi];
            const cgltf_attribute *pos_attr,*nrm_attr,*uv_attr,*tan_attr,*jnt_attr,*wgt_attr;
            size_t vcount,icount;
            ModelcVertex *verts;
            uint32_t *inds;
            int32_t skin_index=node->skin?(int32_t)(node->skin-data->skins):-1;
            int32_t max_bone=-1;
            if(prim->type!=cgltf_primitive_type_triangles&&prim->type!=cgltf_primitive_type_invalid)continue;
            pos_attr=NULL;nrm_attr=NULL;uv_attr=NULL;tan_attr=NULL;jnt_attr=NULL;wgt_attr=NULL;
            for(size_t ai=0;ai<prim->attributes_count;ai++){
                const cgltf_attribute *attr=&prim->attributes[ai];
                if(attr->index!=0)continue;
                if(attr->type==cgltf_attribute_type_position)pos_attr=attr;
                else if(attr->type==cgltf_attribute_type_normal)nrm_attr=attr;
                else if(attr->type==cgltf_attribute_type_texcoord)uv_attr=attr;
                else if(attr->type==cgltf_attribute_type_tangent)tan_attr=attr;
                else if(attr->type==cgltf_attribute_type_joints)jnt_attr=attr;
                else if(attr->type==cgltf_attribute_type_weights)wgt_attr=attr;
            }
            if(!pos_attr||!pos_attr->data||pos_attr->data->count==0)continue;
            vcount=pos_attr->data->count;
            icount=prim->indices?prim->indices->count:vcount;
            verts=(ModelcVertex*)calloc(vcount,sizeof(*verts));
            inds=(uint32_t*)malloc(icount*sizeof(*inds));
            if(!verts||!inds){ free(verts); free(inds); fclose(out); cgltf_free(data); return false; }
            for(size_t vi=0;vi<vcount;vi++){
                cgltf_float fv[4]={0,0,0,1};
                cgltf_uint uv[4]={0,0,0,0};
                ModelcVertex *v=&verts[vi];
                cgltf_accessor_read_float(pos_attr->data,vi,fv,3);
                v->pos[0]=(float)fv[0]; v->pos[1]=(float)fv[1]; v->pos[2]=(float)fv[2];
                if(nrm_attr&&cgltf_accessor_read_float(nrm_attr->data,vi,fv,3)){
                    v->normal[0]=(float)fv[0]; v->normal[1]=(float)fv[1]; v->normal[2]=(float)fv[2];
                }else{
                    v->normal[1]=1.0f;
                }
                if(uv_attr&&cgltf_accessor_read_float(uv_attr->data,vi,fv,2)){
                    v->uv[0]=(float)fv[0]; v->uv[1]=(float)fv[1];
                }
                if(tan_attr&&cgltf_accessor_read_float(tan_attr->data,vi,fv,4)){
                    float sign=fv[3]<0.0f?-1.0f:1.0f;
                    v->tangent[0]=(float)fv[0]; v->tangent[1]=(float)fv[1]; v->tangent[2]=(float)fv[2];
                    v->bitangent[0]=((float)fv[1]*(float)v->normal[2]-(float)fv[2]*(float)v->normal[1])*sign;
                    v->bitangent[1]=((float)fv[2]*(float)v->normal[0]-(float)fv[0]*(float)v->normal[2])*sign;
                    v->bitangent[2]=((float)fv[0]*(float)v->normal[1]-(float)fv[1]*(float)v->normal[0])*sign;
                }else{
                    float nx=v->normal[0],ny=v->normal[1],nz=v->normal[2];
                    float ax=fabsf(ny)<0.99f?0.0f:1.0f;
                    float ay=fabsf(ny)<0.99f?1.0f:0.0f;
                    float dot=ax*nx+ay*ny;
                    float tx=ax-nx*dot,ty=ay-ny*dot,tz=-nz*dot;
                    float len=sqrtf(tx*tx+ty*ty+tz*tz);
                    if(len>1e-5f){ tx/=len; ty/=len; tz/=len; }
                    v->tangent[0]=tx; v->tangent[1]=ty; v->tangent[2]=tz;
                    v->bitangent[0]=ny*tz-nz*ty;
                    v->bitangent[1]=nz*tx-nx*tz;
                    v->bitangent[2]=nx*ty-ny*tx;
                }
                if(jnt_attr&&wgt_attr){
                    float tw=0.0f;
                    cgltf_accessor_read_uint(jnt_attr->data,vi,uv,4);
                    cgltf_accessor_read_float(wgt_attr->data,vi,fv,4);
                    for(int k=0;k<MAX_WEIGHTS;k++){
                        v->bones[k]=(int32_t)uv[k];
                        v->weights[k]=(float)fv[k];
                        if(v->weights[k]>0.0f&&v->bones[k]>max_bone)max_bone=v->bones[k];
                        tw+=v->weights[k];
                    }
                    if(tw>0.001f)for(int k=0;k<MAX_WEIGHTS;k++)v->weights[k]/=tw;
                }
                {
                    float x=(float)world[0]*v->pos[0]+(float)world[4]*v->pos[1]+(float)world[8]*v->pos[2]+(float)world[12];
                    float y=(float)world[1]*v->pos[0]+(float)world[5]*v->pos[1]+(float)world[9]*v->pos[2]+(float)world[13];
                    float z=(float)world[2]*v->pos[0]+(float)world[6]*v->pos[1]+(float)world[10]*v->pos[2]+(float)world[14];
                    if(x<xmin)xmin=x; if(x>xmax)xmax=x;
                    if(y<ymin)ymin=y; if(y>ymax)ymax=y;
                    if(z<zmin)zmin=z; if(z>zmax)zmax=z;
                    if(v->pos[1]<ymin_local)ymin_local=v->pos[1];
                    if(v->pos[1]>ymax_local)ymax_local=v->pos[1];
                }
            }
            for(size_t ii=0;ii<icount;ii++)inds[ii]=prim->indices?(uint32_t)cgltf_accessor_read_index(prim->indices,ii):(uint32_t)ii;

            if(is_static){
                ModelcMeshHeader mh;
                memset(&mh,0,sizeof(mh));
                mh.vertex_count=(uint32_t)vcount;
                mh.index_count=(uint32_t)icount;
                cook_mat4_from_cgltf(mh.geometry_to_world,world);
                mh.node_typed_id=(uint32_t)ni;
                mh.mat_color[0]=mh.mat_color[1]=mh.mat_color[2]=mh.mat_color[3]=1.0f;
                mh.metallic=1.0f;
                mh.roughness=1.0f;
                mh.alpha_cutoff=0.5f;
                if(prim->material){
                    const cgltf_material *mat=prim->material;
                    gltf_texture_source_path(ctx,asset,data,&mat->pbr_metallic_roughness.base_color_texture,mh.texture_path,sizeof(mh.texture_path));
                    gltf_texture_source_path(ctx,asset,data,&mat->normal_texture,mh.normal_path,sizeof(mh.normal_path));
                    gltf_texture_source_path(ctx,asset,data,&mat->pbr_metallic_roughness.metallic_roughness_texture,mh.mr_path,sizeof(mh.mr_path));
                    gltf_texture_source_path(ctx,asset,data,&mat->emissive_texture,mh.emissive_path,sizeof(mh.emissive_path));
                    if(mat->has_pbr_metallic_roughness){
                        mh.mat_color[0]=(float)mat->pbr_metallic_roughness.base_color_factor[0];
                        mh.mat_color[1]=(float)mat->pbr_metallic_roughness.base_color_factor[1];
                        mh.mat_color[2]=(float)mat->pbr_metallic_roughness.base_color_factor[2];
                        mh.mat_color[3]=(float)mat->pbr_metallic_roughness.base_color_factor[3];
                        mh.metallic=(float)mat->pbr_metallic_roughness.metallic_factor;
                        mh.roughness=(float)mat->pbr_metallic_roughness.roughness_factor;
                    }
                    mh.emissive[0]=(float)mat->emissive_factor[0];
                    mh.emissive[1]=(float)mat->emissive_factor[1];
                    mh.emissive[2]=(float)mat->emissive_factor[2];
                    if(mat->alpha_mode==cgltf_alpha_mode_mask)mh.alpha_cutoff=mat->alpha_cutoff>0.0?(float)mat->alpha_cutoff:0.5f;
                    else if(mat->alpha_mode==cgltf_alpha_mode_blend)mh.alpha_cutoff=0.0f;
                    mh.has_pbr=(mh.texture_path[0]||mh.normal_path[0]||mh.mr_path[0]||mh.emissive_path[0]||mat->has_pbr_metallic_roughness);
                }
                if(fwrite(&mh,1,sizeof(mh),out)!=sizeof(mh)||
                   fwrite(verts,sizeof(*verts),vcount,out)!=vcount||
                   fwrite(inds,sizeof(*inds),icount,out)!=icount){
                    free(verts); free(inds); fclose(out); cgltf_free(data); return false;
                }
            }else{
                ModelcMeshHeaderV2 mh;
                memset(&mh,0,sizeof(mh));
                mh.vertex_count=(uint32_t)vcount;
                mh.index_count=(uint32_t)icount;
                cook_mat4_from_cgltf(mh.geometry_to_world,world);
                mh.node_typed_id=(uint32_t)ni;
                mh.skin_index=skin_index;
                mh.num_gpu_bones=max_bone+1;
                if(mh.num_gpu_bones>MAX_BONES)mh.num_gpu_bones=MAX_BONES;
                mh.mat_color[0]=mh.mat_color[1]=mh.mat_color[2]=mh.mat_color[3]=1.0f;
                mh.metallic=1.0f;
                mh.roughness=1.0f;
                mh.alpha_cutoff=0.5f;
                if(prim->material){
                    const cgltf_material *mat=prim->material;
                    gltf_texture_source_path(ctx,asset,data,&mat->pbr_metallic_roughness.base_color_texture,mh.texture_path,sizeof(mh.texture_path));
                    gltf_texture_source_path(ctx,asset,data,&mat->normal_texture,mh.normal_path,sizeof(mh.normal_path));
                    gltf_texture_source_path(ctx,asset,data,&mat->pbr_metallic_roughness.metallic_roughness_texture,mh.mr_path,sizeof(mh.mr_path));
                    gltf_texture_source_path(ctx,asset,data,&mat->emissive_texture,mh.emissive_path,sizeof(mh.emissive_path));
                    if(mat->has_pbr_metallic_roughness){
                        mh.mat_color[0]=(float)mat->pbr_metallic_roughness.base_color_factor[0];
                        mh.mat_color[1]=(float)mat->pbr_metallic_roughness.base_color_factor[1];
                        mh.mat_color[2]=(float)mat->pbr_metallic_roughness.base_color_factor[2];
                        mh.mat_color[3]=(float)mat->pbr_metallic_roughness.base_color_factor[3];
                        mh.metallic=(float)mat->pbr_metallic_roughness.metallic_factor;
                        mh.roughness=(float)mat->pbr_metallic_roughness.roughness_factor;
                    }
                    mh.emissive[0]=(float)mat->emissive_factor[0];
                    mh.emissive[1]=(float)mat->emissive_factor[1];
                    mh.emissive[2]=(float)mat->emissive_factor[2];
                    if(mat->alpha_mode==cgltf_alpha_mode_mask)mh.alpha_cutoff=mat->alpha_cutoff>0.0?(float)mat->alpha_cutoff:0.5f;
                    else if(mat->alpha_mode==cgltf_alpha_mode_blend)mh.alpha_cutoff=0.0f;
                    mh.has_pbr=(mh.texture_path[0]||mh.normal_path[0]||mh.mr_path[0]||mh.emissive_path[0]||mat->has_pbr_metallic_roughness);
                }
                if(fwrite(&mh,1,sizeof(mh),out)!=sizeof(mh)||
                   fwrite(verts,sizeof(*verts),vcount,out)!=vcount||
                   fwrite(inds,sizeof(*inds),icount,out)!=icount){
                    free(verts); free(inds); fclose(out); cgltf_free(data); return false;
                }
            }
            free(verts);
            free(inds);
            mesh_count++;
        }
    }

    if(!is_static){
        ModelcRigHeader rig={0};
        rig.node_count=(uint32_t)data->nodes_count;
        rig.skin_count=(uint32_t)data->skins_count;
        rig.anim_count=(uint32_t)data->animations_count;
        if(fwrite(&rig,1,sizeof(rig),out)!=sizeof(rig)){ fclose(out); cgltf_free(data); return false; }
        for(size_t ni=0;ni<data->nodes_count;ni++){
            ModelcNodeRecord rec;
            cgltf_node *node=&data->nodes[ni];
            cgltf_float world[16];
            memset(&rec,0,sizeof(rec));
            rec.parent_index=node->parent?(int32_t)(node->parent-data->nodes):-1;
            rec.s[0]=rec.s[1]=rec.s[2]=1.0f;
            rec.r[3]=1.0f;
            if(node->has_translation){ rec.t[0]=(float)node->translation[0]; rec.t[1]=(float)node->translation[1]; rec.t[2]=(float)node->translation[2]; }
            if(node->has_scale){ rec.s[0]=(float)node->scale[0]; rec.s[1]=(float)node->scale[1]; rec.s[2]=(float)node->scale[2]; }
            if(node->has_rotation){ rec.r[0]=(float)node->rotation[0]; rec.r[1]=(float)node->rotation[1]; rec.r[2]=(float)node->rotation[2]; rec.r[3]=(float)node->rotation[3]; }
            cgltf_node_transform_world(node,world);
            cook_mat4_from_cgltf(rec.rest_world,world);
            if(fwrite(&rec,1,sizeof(rec),out)!=sizeof(rec)){ fclose(out); cgltf_free(data); return false; }
        }
        for(size_t si=0;si<data->skins_count;si++){
            ModelcSkinRecord rec;
            cgltf_skin *skin=&data->skins[si];
            memset(&rec,0,sizeof(rec));
            rec.joint_count=(int32_t)(skin->joints_count<(cgltf_size)MAX_BONES?skin->joints_count:(cgltf_size)MAX_BONES);
            for(int ji=0;ji<rec.joint_count;ji++){
                rec.joint_nodes[ji]=(int32_t)(skin->joints[ji]-data->nodes);
                for(int mi=0;mi<16;mi++)rec.inverse_bind[ji][mi]=(mi%5)==0?1.0f:0.0f;
                if(skin->inverse_bind_matrices&&ji<(int)skin->inverse_bind_matrices->count){
                    cgltf_float m[16]={0};
                    if(cgltf_accessor_read_float(skin->inverse_bind_matrices,(cgltf_size)ji,m,16))for(int mi=0;mi<16;mi++)rec.inverse_bind[ji][mi]=(float)m[mi];
                }
            }
            if(fwrite(&rec,1,sizeof(rec),out)!=sizeof(rec)){ fclose(out); cgltf_free(data); return false; }
        }
        for(size_t ai=0;ai<data->animations_count;ai++){
            ModelcAnimRecord arec;
            cgltf_animation *anim=&data->animations[ai];
            memset(&arec,0,sizeof(arec));
            if(anim->name)snprintf(arec.name,sizeof(arec.name),"%s",anim->name);
            arec.duration=cook_gltf_anim_duration(anim);
            arec.channel_count=(uint32_t)anim->channels_count;
            if(fwrite(&arec,1,sizeof(arec),out)!=sizeof(arec)){ fclose(out); cgltf_free(data); return false; }
            for(size_t ci=0;ci<anim->channels_count;ci++){
                ModelcAnimChannelRecord crec;
                cgltf_animation_channel *ch=&anim->channels[ci];
                cgltf_accessor *tin,*tout;
                uint32_t key_count;
                size_t value_stride;
                float *times=NULL;
                float (*values)[4]=NULL;
                if(!ch->sampler||!ch->sampler->input||!ch->sampler->output||!ch->target_node||ch->target_path==cgltf_animation_path_type_weights){
                    memset(&crec,0,sizeof(crec));
                    if(fwrite(&crec,1,sizeof(crec),out)!=sizeof(crec)){ fclose(out); cgltf_free(data); return false; }
                    continue;
                }
                tin=ch->sampler->input;
                tout=ch->sampler->output;
                key_count=(uint32_t)tin->count;
                value_stride=(ch->target_path==cgltf_animation_path_type_rotation)?4u:3u;
                memset(&crec,0,sizeof(crec));
                crec.node_index=(uint32_t)(ch->target_node-data->nodes);
                crec.path=(ch->target_path==cgltf_animation_path_type_rotation)?1u:(ch->target_path==cgltf_animation_path_type_scale?2u:0u);
                crec.interpolation=(ch->sampler->interpolation==cgltf_interpolation_type_step)?1u:(ch->sampler->interpolation==cgltf_interpolation_type_cubic_spline?2u:0u);
                crec.key_count=key_count;
                times=key_count?malloc((size_t)key_count*sizeof(*times)):NULL;
                values=key_count?malloc((size_t)key_count*sizeof(*values)):NULL;
                if((key_count&&!times)||(key_count&&!values)){ free(times); free(values); fclose(out); cgltf_free(data); return false; }
                for(uint32_t ki=0;ki<key_count;ki++){
                    cgltf_float tf=0.0;
                    cgltf_float vec[4]={0,0,0,1};
                    size_t sample_index=(crec.interpolation==2u)?(size_t)ki*3u+1u:(size_t)ki;
                    cgltf_accessor_read_float(tin,ki,&tf,1);
                    cgltf_accessor_read_float(tout,sample_index,vec,(cgltf_size)value_stride);
                    times[ki]=(float)tf;
                    values[ki][0]=(float)vec[0];
                    values[ki][1]=(float)vec[1];
                    values[ki][2]=(float)vec[2];
                    values[ki][3]=(float)(value_stride==4u?vec[3]:1.0f);
                }
                if(fwrite(&crec,1,sizeof(crec),out)!=sizeof(crec)||
                   (key_count&&fwrite(times,sizeof(*times),key_count,out)!=key_count)||
                   (key_count&&fwrite(values,sizeof(*values),key_count,out)!=key_count)){
                    free(times); free(values); fclose(out); cgltf_free(data); return false;
                }
                free(times);
                free(values);
            }
        }
    }

    header.mesh_count=mesh_count;
    header.cx=(xmin+xmax)*0.5f;
    header.cy=(ymin+ymax)*0.5f;
    header.cz=(zmin+zmax)*0.5f;
    header.ymin_raw=ymin_local;
    header.ymax_raw=ymax_local;
    {
        float md=fmaxf(xmax-xmin,fmaxf(ymax-ymin,zmax-zmin));
        if(md<0.0001f)md=1.0f;
        header.model_scale=2.0f/md;
        header.ground_offset=-(ymin-header.cy)*header.model_scale;
    }
    rewind(out);
    fwrite(&header,1,sizeof(header),out);
    fclose(out);
    cgltf_free(data);
    return mesh_count>0;
}

static bool gather_gltf_dependencies(CookContext *ctx,CookedAsset *asset){
    cgltf_options opts={0};
    cgltf_data *data=NULL;
    cgltf_result res=cgltf_parse_file(&opts,asset->source,&data);
    if(res!=cgltf_result_success||!data){
        fprintf(stderr,"[Cook] glTF parse failed for %s\n",asset->source);
        return false;
    }
    for(size_t i=0;i<data->images_count;i++){
        cgltf_image *img=&data->images[i];
        char abs_path[MAX_PATH_LEN];
        if(img->uri&&resolve_model_relative(abs_path,sizeof(abs_path),asset->source,img->uri))
            import_dependency(ctx,asset,abs_path);
    }
    if(data->buffers){
        for(size_t i=0;i<data->buffers_count;i++){
            cgltf_buffer *buf=&data->buffers[i];
            char abs_path[MAX_PATH_LEN];
            if(buf->uri&&resolve_model_relative(abs_path,sizeof(abs_path),asset->source,buf->uri))
                import_dependency(ctx,asset,abs_path);
        }
    }
    cgltf_free(data);
    return true;
}

static bool write_model_meta(CookContext *ctx,CookedAsset *asset){
    FILE *f;
    mkdirs_for_file(asset->meta);
    f=fopen(asset->meta,"w");
    if(!f)return false;
    write_meta_header(f,asset);
    fprintf(f,"deps=");
    for(int i=0;i<asset->dep_count;i++){
        CookedAsset *dep=&ctx->assets[asset->deps[i]];
        if(i) fputc(';',f);
        fputs(dep->source,f);
    }
    fputc('\n',f);
    fclose(f);
    return true;
}

static bool import_asset(CookContext *ctx,int asset_idx){
    CookedAsset *asset=&ctx->assets[asset_idx];
    uint64_t hash;
    if(asset->imported) return true;
    if(!file_exists(asset->source)){
        fprintf(stderr,"[Cook] missing asset %s\n",asset->source);
        return false;
    }

    hash=hash_file_fnv1a64(asset->source);
    hash_to_hex(hash,asset->hash);

    if(asset->kind==ASSET_MODEL&&strrchr(asset->source,'.')&&
       (strcasecmp(strrchr(asset->source,'.'),".fbx")==0||strcasecmp(strrchr(asset->source,'.'),".glb")==0)){
        if(!write_cooked_model_bundle(asset->source,asset->cooked)) return false;
    }else{
        if(!copy_file(asset->source,asset->cooked)) return false;
    }

    switch(asset->kind){
        case ASSET_MODEL:
            if(path_has_extension(asset->source)){
                const char *dot=strrchr(asset->source,'.');
                if(dot&&strcasecmp(dot,".fbx")==0){
                    if(export_static_fbx_modelc(ctx,asset)){
                        asset->imported=true;
                        if(!write_model_meta(ctx,asset)) return false;
                        printf("[Cook] %-7s %s -> %s\n",asset_kind_name(asset->kind),asset->source,asset->cooked);
                        return true;
                    }
                    asset_set_cooked_ext(asset,".fbbin");
                    gather_fbx_dependencies(ctx,asset);
                }else if(dot&&(strcasecmp(dot,".glb")==0||strcasecmp(dot,".gltf")==0)){
                    if(export_static_gltf_modelc(ctx,asset)){
                        asset->imported=true;
                        if(!write_model_meta(ctx,asset)) return false;
                        printf("[Cook] %-7s %s -> %s\n",asset_kind_name(asset->kind),asset->source,asset->cooked);
                        return true;
                    }
                    if(strcasecmp(dot,".glb")==0)asset_set_cooked_ext(asset,".glbbin");
                    gather_gltf_dependencies(ctx,asset);
                }
            }
            if(!write_model_meta(ctx,asset)) return false;
            break;
        case ASSET_TEXTURE:
            if(!write_texture_meta(ctx,asset)) return false;
            break;
        case ASSET_AUDIO:
            if(!write_audio_meta(ctx,asset)) return false;
            break;
        default:
            if(!write_generic_meta(ctx,asset)) return false;
            break;
    }

    asset->imported=true;
    printf("[Cook] %-7s %s -> %s\n",asset_kind_name(asset->kind),asset->source,asset->cooked);
    return true;
}

static void discover_path(CookContext *ctx,const char *path){
    struct stat st;
    if(stat(path,&st)!=0){
        fprintf(stderr,"[Cook] missing %s\n",path);
        return;
    }
    if(S_ISDIR(st.st_mode)){
        DIR *dir=opendir(path);
        struct dirent *de;
        if(!dir){
            fprintf(stderr,"[Cook] cannot open dir %s: %s\n",path,strerror(errno));
            return;
        }
        while((de=readdir(dir))){
            char child[MAX_PATH_LEN];
            if(strcmp(de->d_name,".")==0||strcmp(de->d_name,"..")==0) continue;
            snprintf(child,sizeof(child),"%s/%s",path,de->d_name);
            discover_path(ctx,child);
        }
        closedir(dir);
    }else if(S_ISREG(st.st_mode)&&is_cookable_file(path)){
        AssetKind kind=classify_asset(path);
        int idx=ctx_get_or_add_asset(ctx,path,kind);
        if(idx>=0) import_asset(ctx,idx);
    }
}

int main(int argc,char **argv){
    CookContext *ctx;
    char manifest_path[MAX_PATH_LEN];
    if(argc<3){
        fprintf(stderr,"Usage: %s <out_dir> <asset_or_dir> [more assets...]\n",argv[0]);
        return 1;
    }

    ctx=(CookContext*)calloc(1,sizeof(*ctx));
    if(!ctx){
        fprintf(stderr,"[Cook] out of memory allocating cook context\n");
        return 1;
    }
    snprintf(ctx->out_root,sizeof(ctx->out_root),"%s",argv[1]);
    path_normalize(ctx->out_root);
    mkdir_one(ctx->out_root);

    for(int i=2;i<argc;i++) discover_path(ctx,argv[i]);

    path_join(manifest_path,sizeof(manifest_path),ctx->out_root,"asset_manifest.csv");
    ctx->manifest=fopen(manifest_path,"w");
    if(!ctx->manifest){
        fprintf(stderr,"[Cook] cannot create manifest %s: %s\n",manifest_path,strerror(errno));
        free(ctx);
        return 1;
    }
    for(int i=0;i<ctx->asset_count;i++) manifest_write_asset(ctx,&ctx->assets[i]);
    fclose(ctx->manifest);

    printf("[Cook] Wrote %d assets to %s\n",ctx->asset_count,ctx->out_root);
    printf("[Cook] Manifest written to %s\n",manifest_path);
    free(ctx);
    return 0;
}

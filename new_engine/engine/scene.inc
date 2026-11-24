/* ── GPU Mesh / Scene ───────────────────────────────────────── */
#include "engine/texture_cache.h"
typedef struct{float pos[3],normal[3],uv[2];float weights[MAX_WEIGHTS];int32_t bones[MAX_WEIGHTS];float tangent[3],bitangent[3];}Vertex;
typedef struct{
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    GLsizei index_count;
    int32_t cluster_map[MAX_BONES],num_gpu_bones;
    Mat4 geometry_to_world;
    uint32_t node_typed_id;
    GLuint texture,normal_tex,mr_tex,emissive_tex;
    float mat_color[4],emissive[3],metallic,roughness,alpha_cutoff;
    uint8_t tint_region;
    bool has_pbr;
    bool double_sided;
}GpuMesh;
typedef enum{SCENE_FORMAT_NONE=0,SCENE_FORMAT_UFBX,SCENE_FORMAT_GLTF,SCENE_FORMAT_MODELC}SceneFormat;
typedef struct{float x,y,z,w;}Quat;
typedef struct{Vec3 t,s;Quat r;}GltfNodePose;
typedef struct{
    int joint_count;
    int joint_nodes[MAX_BONES];
    Mat4 inverse_bind[MAX_BONES];
}GltfSkinInfo;
typedef struct{
    uint32_t path;
    uint32_t interpolation;
    uint32_t node_index;
    uint32_t key_count;
    float *times;
    float (*values)[4];
}ModelcAnimChannel;
typedef struct{
    char name[64];
    float duration;
    uint32_t channel_count;
    ModelcAnimChannel *channels;
}ModelcAnimation;
typedef struct{
    const cgltf_image *image;
    bool srgb;
    GLuint tex;
}GltfTexCacheEntry;
typedef struct{
    ufbx_scene *scene;
    cgltf_data *gltf;
    void *source_blob;
    size_t source_blob_size;
    SceneFormat format;
    GpuMesh meshes[MAX_MESHES];
    int num_meshes;
    ufbx_skin_deformer *skins[MAX_MESHES];
    int gltf_mesh_skin[MAX_MESHES];
    int gltf_node_count;
    GltfNodePose *gltf_bind_pose;
    Mat4 *gltf_rest_world;
    GltfNodePose *gltf_eval_pose;
    Mat4 *gltf_eval_world;
    uint8_t *gltf_eval_mark;
    GltfSkinInfo *gltf_skins;
    int gltf_skin_count;
    int modelc_node_count;
    int *modelc_parent_index;
    GltfNodePose *modelc_bind_pose;
    Mat4 *modelc_rest_world;
    GltfNodePose *modelc_eval_pose;
    Mat4 *modelc_eval_world;
    uint8_t *modelc_eval_mark;
    GltfSkinInfo *modelc_skins;
    int modelc_skin_count;
    int modelc_mesh_skin[MAX_MESHES];
    ModelcAnimation *modelc_anims;
    int modelc_anim_count;
    GltfTexCacheEntry *gltf_tex_cache;
    int gltf_tex_cache_count;
    int gltf_tex_cache_cap;
    float model_scale,cx,cy,cz,ground_offset,ymin_raw,ymax_raw;
}Scene;

typedef struct {
    ufbx_vec3 *normals;
    uint32_t *indices;
    size_t num_normals;
} MeshNormals;

static uint8_t scene_guess_tint_region_asset_override(const char *asset_path,const char *node_name,const char *mat_name){
    const char *path=asset_path?asset_path:"";
    const char *node=node_name?node_name:"";
    const char *mat=mat_name?mat_name:"";
    if(path_contains_ci(path,"base_mesh")){
        if(path_contains_ci(node,"upper_cloth")||path_contains_ci(node,"lower_cloth")||
           path_contains_ci(node,"cloth")||path_contains_ci(mat,"upper_cloth")||
           path_contains_ci(mat,"lower_cloth")||path_contains_ci(mat,"cloth")){
            return 1;
        }
        if(path_contains_ci(node,"body")||path_contains_ci(node,"face")||
           path_contains_ci(node,"head")||path_contains_ci(mat,"body")||
           path_contains_ci(mat,"face")||path_contains_ci(mat,"head")){
            return 4;
        }
        if(path_contains_ci(node,"hair")||path_contains_ci(mat,"hair")){
            return 5;
        }
        if(path_contains_ci(node,"boot")||path_contains_ci(node,"shoe")||
           path_contains_ci(node,"belt")||path_contains_ci(node,"pouch")||
           path_contains_ci(mat,"boot")||path_contains_ci(mat,"shoe")||
           path_contains_ci(mat,"belt")||path_contains_ci(mat,"pouch")){
            return 2;
        }
        if(path_contains_ci(node,"buckle")||path_contains_ci(node,"metal")||
           path_contains_ci(node,"sword")||path_contains_ci(node,"dagger")||
           path_contains_ci(node,"staff")||path_contains_ci(node,"wand")||
           path_contains_ci(mat,"buckle")||path_contains_ci(mat,"metal")||
           path_contains_ci(mat,"sword")||path_contains_ci(mat,"dagger")||
           path_contains_ci(mat,"staff")||path_contains_ci(mat,"wand")){
            return 3;
        }
    }
    return 0;
}

static uint8_t scene_guess_tint_region(const char *node_name,const char *mat_name,const char *tex_path){
    const char *node=node_name?node_name:"";
    const char *mat=mat_name?mat_name:"";
    const char *tex=tex_path?tex_path:"";
    uint8_t asset_override=scene_guess_tint_region_asset_override(tex_path,node_name,mat_name);
    if(asset_override) return asset_override;
    if(path_contains_ci(node,"skin")||path_contains_ci(node,"head")||path_contains_ci(node,"face")||
       path_contains_ci(node,"hand")||path_contains_ci(node,"body")||
       path_contains_ci(mat,"skin")||path_contains_ci(mat,"face")||path_contains_ci(mat,"body")||
       path_contains_ci(tex,"skin")||path_contains_ci(tex,"face")||path_contains_ci(tex,"body")){
        return 4;
    }
    if(path_contains_ci(node,"plate")||path_contains_ci(node,"chain")||path_contains_ci(node,"metal")||
       path_contains_ci(node,"sword")||path_contains_ci(node,"spear")||path_contains_ci(node,"shield")||
       path_contains_ci(node,"dagger")||path_contains_ci(node,"staff")||path_contains_ci(node,"wand")||
       path_contains_ci(mat,"plate")||path_contains_ci(mat,"chain")||path_contains_ci(mat,"metal")||
       path_contains_ci(mat,"dagger")||path_contains_ci(mat,"staff")||path_contains_ci(mat,"wand")||
       path_contains_ci(tex,"plate")||path_contains_ci(tex,"chain")||path_contains_ci(tex,"metal")||
       path_contains_ci(tex,"dagger")||path_contains_ci(tex,"staff")||path_contains_ci(tex,"wand")){
        return 3;
    }
    if(path_contains_ci(node,"leather")||path_contains_ci(node,"belt")||path_contains_ci(node,"boot")||
       path_contains_ci(node,"shoe")||path_contains_ci(node,"glove")||path_contains_ci(node,"pouch")||
       path_contains_ci(mat,"leather")||path_contains_ci(mat,"belt")||path_contains_ci(mat,"boot")||
       path_contains_ci(tex,"leather")||path_contains_ci(tex,"belt")||path_contains_ci(tex,"boot")||
       path_contains_ci(tex,"shoe")){
        return 2;
    }
    if(path_contains_ci(node,"cloth")||path_contains_ci(node,"robe")||path_contains_ci(node,"tunic")||
       path_contains_ci(node,"shirt")||path_contains_ci(node,"cape")||path_contains_ci(node,"hood")||
       path_contains_ci(node,"upper_cloth")||path_contains_ci(node,"lower_cloth")||
       path_contains_ci(node,"sleeve")||path_contains_ci(node,"pants")||path_contains_ci(node,"skirt")||
       path_contains_ci(mat,"cloth")||path_contains_ci(mat,"robe")||path_contains_ci(mat,"tunic")||
       path_contains_ci(mat,"upper_cloth")||path_contains_ci(mat,"lower_cloth")||
       path_contains_ci(tex,"cloth")||path_contains_ci(tex,"robe")||path_contains_ci(tex,"tunic")){
        return 1;
    }
    if(path_contains_ci(node,"hair")||path_contains_ci(mat,"hair")||path_contains_ci(tex,"hair")){
        return 5;
    }
    return 0;
}

static bool path_has_extension_ci(const char *path,const char *ext);
static bool scene_has_data(const Scene *s);
static int scene_anim_count(const Scene *s);
static float scene_anim_duration(const Scene *s,int anim_index);
static int scene_find_anim(const Scene *s,const char *kw);
static Mat4 mat4_from_cgltf(const cgltf_float *m16);
static const cgltf_attribute *gltf_find_attribute(const cgltf_primitive *prim,cgltf_attribute_type type,int index);
static double anim_stack_real_end(ufbx_anim_stack *stk);
static float gltf_anim_duration(const cgltf_animation *anim);
static bool build_gpu_mesh(GpuMesh *gm,ufbx_skin_deformer **skin_out,ufbx_mesh *mesh,ufbx_node *node,const char *path,size_t material_index);
static bool build_gpu_mesh_gltf(Scene *s,GpuMesh *gm,const cgltf_primitive *prim,const cgltf_node *node,int node_index,int skin_index,const char *path);

static bool path_has_extension_ci(const char *path,const char *ext){
    if(!path||!ext)return false;
    const char *dot=strrchr(path,'.');
    if(!dot)return false;
    while(*dot&&*ext){
        if(tolower((unsigned char)*dot)!=tolower((unsigned char)*ext))return false;
        dot++;ext++;
    }
    return *dot==0&&*ext==0;
}

static bool scene_has_data(const Scene *s){
    return s&&s->format!=SCENE_FORMAT_NONE&&s->num_meshes>0;
}

static int scene_anim_count(const Scene *s){
    if(!s)return 0;
    if(s->format==SCENE_FORMAT_UFBX&&s->scene)return (int)s->scene->anim_stacks.count;
    if(s->format==SCENE_FORMAT_GLTF&&s->gltf)return (int)s->gltf->animations_count;
    if(s->format==SCENE_FORMAT_MODELC)return s->modelc_anim_count;
    return 0;
}

static float scene_anim_duration(const Scene *s,int anim_index){
    if(!s||anim_index<0)return 0.0f;
    if(s->format==SCENE_FORMAT_UFBX&&s->scene&&anim_index<(int)s->scene->anim_stacks.count){
        ufbx_anim_stack *stk=s->scene->anim_stacks.data[anim_index];
        return (float)(anim_stack_real_end(stk)-stk->time_begin);
    }
    if(s->format==SCENE_FORMAT_GLTF&&s->gltf&&anim_index<(int)s->gltf->animations_count){
        return gltf_anim_duration(&s->gltf->animations[anim_index]);
    }
    if(s->format==SCENE_FORMAT_MODELC&&anim_index<s->modelc_anim_count){
        return s->modelc_anims[anim_index].duration;
    }
    return 0.0f;
}

static int scene_find_anim(const Scene *s,const char *kw){
    if(!s||!kw||!kw[0])return -1;
    if(s->format==SCENE_FORMAT_UFBX&&s->scene){
        for(size_t i=0;i<s->scene->anim_stacks.count;i++){
            const char *n=s->scene->anim_stacks.data[i]->name.data;
            for(size_t si=0;n[si];si++){size_t ki=0;while(kw[ki]&&tolower((unsigned char)n[si+ki])==tolower((unsigned char)kw[ki]))ki++;if(!kw[ki])return (int)i;}
        }
        return -1;
    }
    if(s->format==SCENE_FORMAT_GLTF&&s->gltf){
        for(size_t i=0;i<s->gltf->animations_count;i++){
            const char *n=s->gltf->animations[i].name?s->gltf->animations[i].name:"";
            for(size_t si=0;n[si];si++){size_t ki=0;while(kw[ki]&&tolower((unsigned char)n[si+ki])==tolower((unsigned char)kw[ki]))ki++;if(!kw[ki])return (int)i;}
        }
    }
    if(s->format==SCENE_FORMAT_MODELC&&s->modelc_anims){
        for(int i=0;i<s->modelc_anim_count;i++){
            const char *n=s->modelc_anims[i].name;
            for(size_t si=0;n[si];si++){size_t ki=0;while(kw[ki]&&tolower((unsigned char)n[si+ki])==tolower((unsigned char)kw[ki]))ki++;if(!kw[ki])return i;}
        }
    }
    return -1;
}

static MeshNormals mesh_normals_build(ufbx_mesh *mesh){
    MeshNormals mn={0};
    if(mesh->num_indices==0)return mn;
    ufbx_topo_edge *topo=malloc(mesh->num_indices*sizeof(ufbx_topo_edge));
    uint32_t *normal_indices=malloc(mesh->num_indices*sizeof(uint32_t));
    if(!topo||!normal_indices){free(topo);free(normal_indices);return mn;}
    ufbx_compute_topology(mesh,topo,mesh->num_indices);
    size_t num_normals=ufbx_generate_normal_mapping(mesh,topo,mesh->num_indices,normal_indices,mesh->num_indices,false);
    free(topo);
    if(num_normals==0){free(normal_indices);return mn;}
    ufbx_vec3 *normals=malloc(num_normals*sizeof(ufbx_vec3));
    if(!normals){free(normal_indices);return mn;}
    ufbx_compute_normals(mesh,&mesh->vertex_position,normal_indices,mesh->num_indices,normals,num_normals);
    mn.normals=normals;
    mn.indices=normal_indices;
    mn.num_normals=num_normals;
    return mn;
}

static inline ufbx_vec3 mesh_normal_get(const MeshNormals *mn, ufbx_mesh *mesh, uint32_t ix){
    if(mn->normals&&mn->indices&&ix<mesh->num_indices){
        uint32_t ni=mn->indices[ix];
        if(ni<mn->num_normals)return mn->normals[ni];
    }
    if(mesh->vertex_normal.exists)return ufbx_get_vertex_vec3(&mesh->vertex_normal,ix);
    return (ufbx_vec3){0.0,1.0,0.0};
}

static void mesh_normals_free(MeshNormals *mn){
    free(mn->normals);
    free(mn->indices);
    memset(mn,0,sizeof(*mn));
}

static float uv_stream_score(const ufbx_mesh *mesh,const ufbx_vertex_vec2 *uvs){
    if(!mesh||!uvs||!uvs->exists||mesh->num_indices==0)return -1.0f;
    float umin=1e30f,vmin=1e30f,umax=-1e30f,vmax=-1e30f;
    for(size_t ix=0;ix<mesh->num_indices;ix++){
        ufbx_vec2 uv=ufbx_get_vertex_vec2(uvs,ix);
        if((float)uv.x<umin)umin=(float)uv.x;
        if((float)uv.y<vmin)vmin=(float)uv.y;
        if((float)uv.x>umax)umax=(float)uv.x;
        if((float)uv.y>vmax)vmax=(float)uv.y;
    }
    float du=umax-umin,dv=vmax-vmin;
    if(du<1e-5f&&dv<1e-5f)return 0.0f;
    return du*dv;
}

static const ufbx_uv_set *mesh_preferred_uv_set(const ufbx_mesh *mesh){
    if(!mesh)return NULL;
    const ufbx_uv_set *best=NULL;
    float best_score=-1.0f;
    for(size_t i=0;i<mesh->uv_sets.count;i++){
        const ufbx_uv_set *uv=&mesh->uv_sets.data[i];
        float score=uv_stream_score(mesh,&uv->vertex_uv);
        if(score<=0.0f)continue;
        if(path_contains_ci(uv->name.data,"UVMap"))score+=1000.0f;
        if(score>best_score){best=uv;best_score=score;}
    }
    return best;
}

static const ufbx_vertex_vec2 *mesh_preferred_uv(const ufbx_mesh *mesh){
    if(mesh&&mesh->vertex_uv.exists&&uv_stream_score(mesh,&mesh->vertex_uv)>0.0f)return &mesh->vertex_uv;
    const ufbx_uv_set *set=mesh_preferred_uv_set(mesh);
    if(set)return &set->vertex_uv;
    return NULL;
}

static const ufbx_vertex_vec3 *mesh_preferred_tangent(const ufbx_mesh *mesh){
    if(mesh&&mesh->vertex_tangent.exists&&uv_stream_score(mesh,&mesh->vertex_uv)>0.0f)return &mesh->vertex_tangent;
    const ufbx_uv_set *set=mesh_preferred_uv_set(mesh);
    if(set&&set->vertex_tangent.exists)return &set->vertex_tangent;
    return NULL;
}

static const ufbx_vertex_vec3 *mesh_preferred_bitangent(const ufbx_mesh *mesh){
    if(mesh&&mesh->vertex_bitangent.exists&&uv_stream_score(mesh,&mesh->vertex_uv)>0.0f)return &mesh->vertex_bitangent;
    const ufbx_uv_set *set=mesh_preferred_uv_set(mesh);
    if(set&&set->vertex_bitangent.exists)return &set->vertex_bitangent;
    return NULL;
}

static const char *mesh_preferred_uv_name(const ufbx_mesh *mesh){
    if(!mesh)return "<none>";
    if(mesh->vertex_uv.exists&&uv_stream_score(mesh,&mesh->vertex_uv)>0.0f)return "<vertex_uv>";
    const ufbx_uv_set *set=mesh_preferred_uv_set(mesh);
    if(set)return set->name.data;
    return "<none>";
}

static bool texture_path_pick_candidate(char *dst,size_t cap,const char *path){
    char candidate[1024];
    const char *exts[]={"png","jpg","jpeg","tga","bmp",NULL};
    if(!dst||cap==0||!path||!path[0])return false;
    dst[0]=0;
    snprintf(candidate,sizeof(candidate),"%s",path);
    path_normalize_slashes(candidate);
    asset_resolve_path_copy(dst,cap,candidate);
    if(access(dst,F_OK)==0)return true;
    const char *base=strrchr(candidate,'/');
    if(base&&base[1]){
        asset_resolve_path_copy(dst,cap,base+1);
        if(access(dst,F_OK)==0)return true;
    }
    if(path_has_extension(candidate))return false;
    for(int ei=0;exts[ei];ei++){
        char try_path[1100];
        snprintf(try_path,sizeof(try_path),"%s.%s",candidate,exts[ei]);
        asset_resolve_path_copy(dst,cap,try_path);
        if(access(dst,F_OK)==0)return true;
        if(base&&base[1]){
            char try_base[600];
            snprintf(try_base,sizeof(try_base),"%s.%s",base+1,exts[ei]);
            asset_resolve_path_copy(dst,cap,try_base);
            if(access(dst,F_OK)==0)return true;
        }
    }
    return false;
}

/* Forward declarations — defined after GPU mesh code below */
static bool scene_load(Scene *s, const char *path);
static void scene_free(Scene *s);

#define COOKED_FBX_MAGIC 0x31424643u
#define MODELC_MAGIC 0x3143444du
#define MODELC_VERSION 1u
#define MODELC_VERSION_ANIM 2u
#define MODELC_TEX_PATH_MAX 256u

typedef enum{
    MODELC_ANIM_TRANSLATION=0,
    MODELC_ANIM_ROTATION=1,
    MODELC_ANIM_SCALE=2
}ModelcAnimPath;

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
    float weights[MAX_WEIGHTS];
    int32_t bones[MAX_WEIGHTS];
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

static bool read_file_bytes_alloc(const char *path,void **out_data,size_t *out_size){
    FILE *f;
    long len;
    void *data;
    if(!out_data||!out_size)return false;
    *out_data=NULL;
    *out_size=0;
    f=fopen(path,"rb");
    if(!f)return false;
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return false;}
    len=ftell(f);
    if(len<0){fclose(f);return false;}
    if(fseek(f,0,SEEK_SET)!=0){fclose(f);return false;}
    data=malloc((size_t)len);
    if(!data){fclose(f);return false;}
    if(len>0&&fread(data,1,(size_t)len,f)!=(size_t)len){free(data);fclose(f);return false;}
    fclose(f);
    *out_data=data;
    *out_size=(size_t)len;
    return true;
}

static bool scene_load_cooked_fbx(Scene *s,const char *bundle_path){
    void *bundle=NULL;
    size_t bundle_size=0;
    CookedFbxHeader header;
    const char *src_path;
    const uint8_t *payload;
    char logical_path[1024];
    ufbx_load_opts opts={0};
    ufbx_error err;

    if(!read_file_bytes_alloc(bundle_path,&bundle,&bundle_size)) return false;
    if(bundle_size<sizeof(header)){ free(bundle); return false; }
    memcpy(&header,bundle,sizeof(header));
    if(header.magic!=COOKED_FBX_MAGIC||header.version!=1){ free(bundle); return false; }
    if((size_t)header.source_path_len>=sizeof(logical_path)||
       bundle_size<sizeof(header)+(size_t)header.source_path_len+(size_t)header.payload_size){
        free(bundle);
        return false;
    }

    src_path=(const char *)((const uint8_t*)bundle+sizeof(header));
    memcpy(logical_path,src_path,(size_t)header.source_path_len);
    logical_path[header.source_path_len]=0;
    payload=(const uint8_t*)bundle+sizeof(header)+(size_t)header.source_path_len;

    opts.generate_missing_normals=true;
    opts.target_axes=ufbx_axes_right_handed_y_up;
    opts.target_unit_meters=1.0f;
    opts.use_blender_pbr_material=true;
    opts.normalize_tangents=true;
    opts.filename.data=logical_path;
    opts.filename.length=strlen(logical_path);

    s->scene=ufbx_load_memory(payload,(size_t)header.payload_size,&opts,&err);
    if(!s->scene){
        fprintf(stderr,"Load cooked '%s': %s\n",bundle_path,err.description.data);
        free(bundle);
        return false;
    }
    s->source_blob=bundle;
    s->source_blob_size=bundle_size;
    s->format=SCENE_FORMAT_UFBX;
    s->num_meshes=0;
    for(size_t ni=0;ni<s->scene->nodes.count&&s->num_meshes<MAX_MESHES;ni++){
        ufbx_node *nd=s->scene->nodes.data[ni];
        if(!nd->mesh)continue;
        size_t mat_count=nd->mesh->materials.count;
        bool split_by_material=mat_count>1&&nd->mesh->face_material.count==nd->mesh->num_faces;
        size_t passes=split_by_material?mat_count:1;
        for(size_t part=0;part<passes&&s->num_meshes<MAX_MESHES;part++){
            int mi=s->num_meshes++;
            if(!build_gpu_mesh(&s->meshes[mi],&s->skins[mi],nd->mesh,nd,logical_path,split_by_material?part:0))s->num_meshes--;
        }
    }
    {
        float xmin=1e9f,ymin=1e9f,zmin=1e9f,xmax=-1e9f,ymax=-1e9f,zmax=-1e9f;
        for(size_t ni=0;ni<s->scene->nodes.count;ni++){ufbx_node *nd=s->scene->nodes.data[ni];if(!nd->mesh)continue;
            for(size_t vi=0;vi<nd->mesh->num_vertices;vi++){ufbx_vec3 lp=nd->mesh->vertices.data[vi];ufbx_vec3 p=ufbx_transform_position(&nd->geometry_to_world,lp);
                float x=(float)p.x,y=(float)p.y,z=(float)p.z;if(x<xmin)xmin=x;if(x>xmax)xmax=x;if(y<ymin)ymin=y;if(y>ymax)ymax=y;if(z<zmin)zmin=z;if(z>zmax)zmax=z;}}
        float ymin_local=1e9f,ymax_local=-1e9f;
        for(size_t ni=0;ni<s->scene->nodes.count;ni++){ufbx_node *nd=s->scene->nodes.data[ni];if(!nd->mesh)continue;
            for(size_t fi=0;fi<nd->mesh->num_faces;fi++){ufbx_face face=nd->mesh->faces.data[fi];
                for(size_t i=face.index_begin;i<face.index_begin+face.num_indices;i++){
                    float y=(float)ufbx_get_vertex_vec3(&nd->mesh->vertex_position,i).y;
                    if(y<ymin_local)ymin_local=y;if(y>ymax_local)ymax_local=y;}}}
        s->ymin_raw=ymin_local; s->ymax_raw=ymax_local;
        printf("[Scene] %s: local_ymin=%.3f local_ymax=%.3f world_ymin=%.3f world_ymax=%.3f\n",
               logical_path,(double)ymin_local,(double)ymax_local,(double)ymin,(double)ymax);
        s->cx=(xmin+xmax)*0.5f;s->cy=(ymin+ymax)*0.5f;s->cz=(zmin+zmax)*0.5f;
        s->ymin_raw=ymin; s->ymax_raw=ymax;
        {
            float md=fmaxf(xmax-xmin,fmaxf(ymax-ymin,zmax-zmin));if(md<0.0001f)md=1.0f;
            s->model_scale=2.0f/md;s->ground_offset=-(ymin-s->cy)*s->model_scale;
        }
    }
    printf("[Scene] %s: %d meshes (cooked fbx)\n",logical_path,s->num_meshes);
    return true;
}

static bool scene_load_modelc(Scene *s,const char *path){
    void *blob=NULL;
    size_t blob_size=0;
    ModelcHeader header;
    const uint8_t *ptr;
    const uint8_t *end;
    if(!read_file_bytes_alloc(path,&blob,&blob_size)) return false;
    if(blob_size<sizeof(header)){ free(blob); return false; }
    memcpy(&header,blob,sizeof(header));
    if(header.magic!=MODELC_MAGIC||(header.version!=MODELC_VERSION&&header.version!=MODELC_VERSION_ANIM)){ free(blob); return false; }
    memset(s,0,sizeof(*s));
    s->source_blob=blob;
    s->source_blob_size=blob_size;
    s->format=SCENE_FORMAT_MODELC;
    s->model_scale=header.model_scale;
    s->cx=header.cx; s->cy=header.cy; s->cz=header.cz;
    s->ground_offset=header.ground_offset;
    s->ymin_raw=header.ymin_raw; s->ymax_raw=header.ymax_raw;
    ptr=(const uint8_t*)blob+sizeof(header);
    end=(const uint8_t*)blob+blob_size;
    for(uint32_t mi=0;mi<header.mesh_count&&s->num_meshes<MAX_MESHES;mi++){
        GLH_Attrib attrs[]={{0,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,pos)},{1,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,normal)},{2,2,GL_FLOAT,GL_FALSE,offsetof(Vertex,uv)},{3,4,GL_FLOAT,GL_FALSE,offsetof(Vertex,weights)},{4,4,GL_INT,0,offsetof(Vertex,bones)},{5,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,tangent)},{6,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,bitangent)}};
        GpuMesh *gm=&s->meshes[s->num_meshes];
        size_t vbytes, ibytes;
        uint32_t vcount,icount;
        if(header.version>=MODELC_VERSION_ANIM){
            ModelcMeshHeaderV2 mh;
            if((size_t)(end-ptr)<sizeof(mh)){ scene_free(s); return false; }
            memcpy(&mh,ptr,sizeof(mh));
            ptr+=sizeof(mh);
            vcount=mh.vertex_count; icount=mh.index_count;
            memset(gm,0,sizeof(*gm));
            memcpy(gm->geometry_to_world.m,mh.geometry_to_world,sizeof(gm->geometry_to_world.m));
            gm->node_typed_id=mh.node_typed_id;
            gm->num_gpu_bones=mh.num_gpu_bones;
            memcpy(gm->mat_color,mh.mat_color,sizeof(gm->mat_color));
            memcpy(gm->emissive,mh.emissive,sizeof(gm->emissive));
            gm->metallic=mh.metallic;
            gm->roughness=mh.roughness;
            gm->alpha_cutoff=mh.alpha_cutoff;
            gm->has_pbr=mh.has_pbr!=0;
            gm->texture=mh.texture_path[0]?texture_file_try_candidates_mode(mh.texture_path,true):0;
            gm->normal_tex=mh.normal_path[0]?texture_file_try_candidates_mode(mh.normal_path,false):0;
            gm->mr_tex=mh.mr_path[0]?texture_file_try_candidates_mode(mh.mr_path,false):0;
            gm->emissive_tex=mh.emissive_path[0]?texture_file_try_candidates_mode(mh.emissive_path,true):0;
            gm->tint_region=scene_guess_tint_region(NULL,NULL,mh.texture_path);
            s->modelc_mesh_skin[s->num_meshes]=mh.skin_index;
        }else{
            ModelcMeshHeader mh;
            if((size_t)(end-ptr)<sizeof(mh)){ scene_free(s); return false; }
            memcpy(&mh,ptr,sizeof(mh));
            ptr+=sizeof(mh);
            vcount=mh.vertex_count; icount=mh.index_count;
            memset(gm,0,sizeof(*gm));
            memcpy(gm->geometry_to_world.m,mh.geometry_to_world,sizeof(gm->geometry_to_world.m));
            gm->node_typed_id=mh.node_typed_id;
            memcpy(gm->mat_color,mh.mat_color,sizeof(gm->mat_color));
            memcpy(gm->emissive,mh.emissive,sizeof(gm->emissive));
            gm->metallic=mh.metallic;
            gm->roughness=mh.roughness;
            gm->alpha_cutoff=mh.alpha_cutoff;
            gm->has_pbr=mh.has_pbr!=0;
            gm->texture=mh.texture_path[0]?texture_file_try_candidates_mode(mh.texture_path,true):0;
            gm->normal_tex=mh.normal_path[0]?texture_file_try_candidates_mode(mh.normal_path,false):0;
            gm->mr_tex=mh.mr_path[0]?texture_file_try_candidates_mode(mh.mr_path,false):0;
            gm->emissive_tex=mh.emissive_path[0]?texture_file_try_candidates_mode(mh.emissive_path,true):0;
            gm->tint_region=scene_guess_tint_region(NULL,NULL,mh.texture_path);
            s->modelc_mesh_skin[s->num_meshes]=-1;
        }
        vbytes=(size_t)vcount*sizeof(ModelcVertex);
        ibytes=(size_t)icount*sizeof(uint32_t);
        if((size_t)(end-ptr)<vbytes+ibytes){ scene_free(s); return false; }
        gm->vao=GLH_mesh_full(ptr,vbytes,ptr+vbytes,ibytes,sizeof(ModelcVertex),attrs,7,
                              &gm->vbo,&gm->ebo);
        gm->index_count=(GLsizei)icount;
        s->num_meshes++;
        ptr+=vbytes+ibytes;
    }
    if(header.version>=MODELC_VERSION_ANIM){
        ModelcRigHeader rig;
        if((size_t)(end-ptr)<sizeof(rig)){ scene_free(s); return false; }
        memcpy(&rig,ptr,sizeof(rig));
        ptr+=sizeof(rig);
        s->format=SCENE_FORMAT_MODELC;
        s->modelc_node_count=(int)rig.node_count;
        s->modelc_skin_count=(int)rig.skin_count;
        s->modelc_anim_count=(int)rig.anim_count;
        if(s->modelc_node_count>0){
            s->modelc_parent_index=calloc((size_t)s->modelc_node_count,sizeof(*s->modelc_parent_index));
            s->modelc_bind_pose=calloc((size_t)s->modelc_node_count,sizeof(*s->modelc_bind_pose));
            s->modelc_rest_world=calloc((size_t)s->modelc_node_count,sizeof(*s->modelc_rest_world));
            s->modelc_eval_pose=calloc((size_t)s->modelc_node_count,sizeof(*s->modelc_eval_pose));
            s->modelc_eval_world=calloc((size_t)s->modelc_node_count,sizeof(*s->modelc_eval_world));
            s->modelc_eval_mark=calloc((size_t)s->modelc_node_count,sizeof(*s->modelc_eval_mark));
            if(!s->modelc_parent_index||!s->modelc_bind_pose||!s->modelc_rest_world||!s->modelc_eval_pose||!s->modelc_eval_world||!s->modelc_eval_mark){ scene_free(s); return false; }
            for(int i=0;i<s->modelc_node_count;i++){
                ModelcNodeRecord rec;
                if((size_t)(end-ptr)<sizeof(rec)){ scene_free(s); return false; }
                memcpy(&rec,ptr,sizeof(rec));
                ptr+=sizeof(rec);
                s->modelc_parent_index[i]=rec.parent_index;
                s->modelc_bind_pose[i].t=(Vec3){rec.t[0],rec.t[1],rec.t[2]};
                s->modelc_bind_pose[i].s=(Vec3){rec.s[0],rec.s[1],rec.s[2]};
                s->modelc_bind_pose[i].r=(Quat){rec.r[0],rec.r[1],rec.r[2],rec.r[3]};
                memcpy(s->modelc_rest_world[i].m,rec.rest_world,sizeof(rec.rest_world));
                s->modelc_eval_pose[i]=s->modelc_bind_pose[i];
                s->modelc_eval_world[i]=s->modelc_rest_world[i];
            }
        }
        if(s->modelc_skin_count>0){
            s->modelc_skins=calloc((size_t)s->modelc_skin_count,sizeof(*s->modelc_skins));
            if(!s->modelc_skins){ scene_free(s); return false; }
            for(int si=0;si<s->modelc_skin_count;si++){
                ModelcSkinRecord rec;
                if((size_t)(end-ptr)<sizeof(rec)){ scene_free(s); return false; }
                memcpy(&rec,ptr,sizeof(rec));
                ptr+=sizeof(rec);
                s->modelc_skins[si].joint_count=rec.joint_count;
                memcpy(s->modelc_skins[si].joint_nodes,rec.joint_nodes,sizeof(rec.joint_nodes));
                for(int ji=0;ji<MAX_BONES;ji++)memcpy(s->modelc_skins[si].inverse_bind[ji].m,rec.inverse_bind[ji],sizeof(rec.inverse_bind[ji]));
            }
        }
        if(s->modelc_anim_count>0){
            s->modelc_anims=calloc((size_t)s->modelc_anim_count,sizeof(*s->modelc_anims));
            if(!s->modelc_anims){ scene_free(s); return false; }
            for(int ai=0;ai<s->modelc_anim_count;ai++){
                ModelcAnimRecord arec;
                if((size_t)(end-ptr)<sizeof(arec)){ scene_free(s); return false; }
                memcpy(&arec,ptr,sizeof(arec));
                ptr+=sizeof(arec);
                memcpy(s->modelc_anims[ai].name,arec.name,sizeof(arec.name));
                s->modelc_anims[ai].duration=arec.duration;
                s->modelc_anims[ai].channel_count=arec.channel_count;
                if(arec.channel_count){
                    s->modelc_anims[ai].channels=calloc((size_t)arec.channel_count,sizeof(*s->modelc_anims[ai].channels));
                    if(!s->modelc_anims[ai].channels){ scene_free(s); return false; }
                }
                for(uint32_t ci=0;ci<arec.channel_count;ci++){
                    ModelcAnimChannelRecord crec;
                    ModelcAnimChannel *dst=&s->modelc_anims[ai].channels[ci];
                    size_t tbytes,vbytes2;
                    if((size_t)(end-ptr)<sizeof(crec)){ scene_free(s); return false; }
                    memcpy(&crec,ptr,sizeof(crec));
                    ptr+=sizeof(crec);
                    dst->node_index=crec.node_index;
                    dst->path=crec.path;
                    dst->interpolation=crec.interpolation;
                    dst->key_count=crec.key_count;
                    tbytes=(size_t)crec.key_count*sizeof(float);
                    vbytes2=(size_t)crec.key_count*sizeof(*dst->values);
                    if((size_t)(end-ptr)<tbytes+vbytes2){ scene_free(s); return false; }
                    dst->times=malloc(tbytes);
                    dst->values=malloc(vbytes2);
                    if((!dst->times&&tbytes)||(!dst->values&&vbytes2)){ scene_free(s); return false; }
                    memcpy(dst->times,ptr,tbytes);
                    ptr+=tbytes;
                    memcpy(dst->values,ptr,vbytes2);
                    ptr+=vbytes2;
                }
            }
        }
    }
    printf("[Scene] %s: %d meshes (modelc v%u)\n",path,s->num_meshes,header.version);
    return s->num_meshes>0;
}

static bool scene_load_cooked_glb(Scene *s,const char *bundle_path){
    void *bundle=NULL;
    size_t bundle_size=0;
    CookedFbxHeader header;
    const char *src_path;
    const uint8_t *payload;
    char logical_path[1024];
    cgltf_options opts={0};
    cgltf_result res;

    if(!read_file_bytes_alloc(bundle_path,&bundle,&bundle_size)) return false;
    if(bundle_size<sizeof(header)){ free(bundle); return false; }
    memcpy(&header,bundle,sizeof(header));
    if(header.magic!=COOKED_FBX_MAGIC||header.version!=1){ free(bundle); return false; }
    if((size_t)header.source_path_len>=sizeof(logical_path)||
       bundle_size<sizeof(header)+(size_t)header.source_path_len+(size_t)header.payload_size){
        free(bundle);
        return false;
    }

    src_path=(const char *)((const uint8_t*)bundle+sizeof(header));
    memcpy(logical_path,src_path,(size_t)header.source_path_len);
    logical_path[header.source_path_len]=0;
    payload=(const uint8_t*)bundle+sizeof(header)+(size_t)header.source_path_len;

    memset(s,0,sizeof(*s));
    res=cgltf_parse(&opts,payload,(cgltf_size)header.payload_size,&s->gltf);
    if(res!=cgltf_result_success||!s->gltf){
        fprintf(stderr,"Load cooked '%s': cgltf_parse failed (%d)\n",bundle_path,(int)res);
        free(bundle);
        return false;
    }
    res=cgltf_load_buffers(&opts,s->gltf,logical_path);
    if(res!=cgltf_result_success){
        cgltf_free(s->gltf);
        s->gltf=NULL;
        free(bundle);
        fprintf(stderr,"Load cooked '%s': cgltf_load_buffers failed (%d)\n",bundle_path,(int)res);
        return false;
    }
    s->source_blob=bundle;
    s->source_blob_size=bundle_size;
    s->format=SCENE_FORMAT_GLTF;
    s->num_meshes=0;
    s->gltf_node_count=(int)s->gltf->nodes_count;
    if(s->gltf_node_count>0){
        s->gltf_bind_pose=calloc((size_t)s->gltf_node_count,sizeof(GltfNodePose));
        s->gltf_rest_world=calloc((size_t)s->gltf_node_count,sizeof(Mat4));
        s->gltf_eval_pose=calloc((size_t)s->gltf_node_count,sizeof(GltfNodePose));
        s->gltf_eval_world=calloc((size_t)s->gltf_node_count,sizeof(Mat4));
        s->gltf_eval_mark=calloc((size_t)s->gltf_node_count,sizeof(uint8_t));
        if(!s->gltf_bind_pose||!s->gltf_rest_world||!s->gltf_eval_pose||!s->gltf_eval_world||!s->gltf_eval_mark){
            scene_free(s);
            return false;
        }
        for(int i=0;i<s->gltf_node_count;i++){
            cgltf_node *node=&s->gltf->nodes[i];
            GltfNodePose pose={{0,0,0},{1,1,1},{0,0,0,1}};
            if(node->has_translation){pose.t.x=(float)node->translation[0];pose.t.y=(float)node->translation[1];pose.t.z=(float)node->translation[2];}
            if(node->has_scale){pose.s.x=(float)node->scale[0];pose.s.y=(float)node->scale[1];pose.s.z=(float)node->scale[2];}
            if(node->has_rotation){pose.r.x=(float)node->rotation[0];pose.r.y=(float)node->rotation[1];pose.r.z=(float)node->rotation[2];pose.r.w=(float)node->rotation[3];}
            s->gltf_bind_pose[i]=pose;
            s->gltf_eval_pose[i]=pose;
        }
    }
    s->gltf_skin_count=(int)s->gltf->skins_count;
    if(s->gltf_skin_count>0){
        s->gltf_skins=calloc((size_t)s->gltf_skin_count,sizeof(GltfSkinInfo));
        if(!s->gltf_skins){scene_free(s);return false;}
        for(int si=0;si<s->gltf_skin_count;si++){
            cgltf_skin *skin=&s->gltf->skins[si];
            GltfSkinInfo *info=&s->gltf_skins[si];
            info->joint_count=(int)(skin->joints_count<MAX_BONES?skin->joints_count:MAX_BONES);
            for(int ji=0;ji<info->joint_count;ji++) info->joint_nodes[ji]=(int)(skin->joints[ji]-s->gltf->nodes);
            if(skin->inverse_bind_matrices&&skin->inverse_bind_matrices->count>0){
                for(int ji=0;ji<info->joint_count&&ji<(int)skin->inverse_bind_matrices->count;ji++){
                    cgltf_float m[16];
                    cgltf_accessor_read_float(skin->inverse_bind_matrices,(cgltf_size)ji,m,16);
                    info->inverse_bind[ji]=mat4_from_cgltf(m);
                }
            }else{
                for(int ji=0;ji<info->joint_count;ji++) info->inverse_bind[ji]=mat4_identity();
            }
        }
    }
    if(s->gltf->images_count>0){
        cgltf_size want=s->gltf->images_count*2;
        if(want<8)want=8;
        if(want>MAX_GLTF_TEX_CACHE)want=MAX_GLTF_TEX_CACHE;
        s->gltf_tex_cache=calloc((size_t)want,sizeof(GltfTexCacheEntry));
        if(s->gltf_tex_cache){
            s->gltf_tex_cache_cap=(int)want;
            s->gltf_tex_cache_count=0;
        }
    }
    for(int ni=0;ni<s->gltf_node_count&&s->num_meshes<MAX_MESHES;ni++){
        cgltf_node *node=&s->gltf->nodes[ni];
        if(!node->mesh)continue;
        {
            int skin_index=node->skin?(int)(node->skin-s->gltf->skins):-1;
            for(cgltf_size pi=0;pi<node->mesh->primitives_count&&s->num_meshes<MAX_MESHES;pi++){
                int mi=s->num_meshes++;
                s->gltf_mesh_skin[mi]=skin_index;
                if(!build_gpu_mesh_gltf(s,&s->meshes[mi],&node->mesh->primitives[pi],node,ni,skin_index,logical_path))s->num_meshes--;
            }
        }
    }
    if(s->num_meshes==0){scene_free(s);return false;}
    {
        float xmin=1e9f,ymin=1e9f,zmin=1e9f,xmax=-1e9f,ymax=-1e9f,zmax=-1e9f;
        for(int mi=0;mi<s->num_meshes;mi++){
            Vec3 p={s->meshes[mi].geometry_to_world.m[12],s->meshes[mi].geometry_to_world.m[13],s->meshes[mi].geometry_to_world.m[14]};
            if(p.x<xmin)xmin=p.x;if(p.x>xmax)xmax=p.x;
            if(p.y<ymin)ymin=p.y;if(p.y>ymax)ymax=p.y;
            if(p.z<zmin)zmin=p.z;if(p.z>zmax)zmax=p.z;
        }
        s->cx=(xmin+xmax)*0.5f;s->cy=(ymin+ymax)*0.5f;s->cz=(zmin+zmax)*0.5f;
        s->ymin_raw=ymin; s->ymax_raw=ymax;
        {
            float md=fmaxf(xmax-xmin,fmaxf(ymax-ymin,zmax-zmin));if(md<0.0001f)md=1.0f;
            s->model_scale=2.0f/md;s->ground_offset=-(ymin-s->cy)*s->model_scale;
        }
    }
    printf("[Scene] %s: %d meshes (cooked glb)\n",logical_path,s->num_meshes);
    return true;
}
#include "engine/charmodel.inc"

static ufbx_material *mesh_material_at(ufbx_mesh *mesh,ufbx_node *node,size_t index){
    if(node&&index<node->materials.count){
        ufbx_material *mat=node->materials.data[index];
        if(mat)return mat;
    }
    if(mesh&&index<mesh->materials.count)return mesh->materials.data[index];
    return NULL;
}

static GLuint get_knight_override_tex(ufbx_node *node){
    char node_name[256]={0};
    const char *const plates[]={
        "textures/knight_plates_BaseColor.png",
        "models/characters/knight_3/textures(4)/textures/knight_plates_BaseColor.png",
    };
    const char *const leather[]={
        "textures/knight_leather_BaseColor.png",
        "models/characters/knight_3/textures(4)/textures/knight_leather_BaseColor.png",
    };
    const char *const chainmail[]={
        "textures/knight_chainmail_BaseColor.png",
        "models/characters/knight_3/textures(4)/textures/knight_chainmail_BaseColor.png",
    };
    const char *const belts[]={
        "models/characters/knight_3/textures(4)/textures/belts_belts_BaseColor.png",
    };
    const char *const sword[]={
        "models/characters/knight_3/textures(4)/textures/ZWEIHANDER_ZWEIHANDER_BaseColor.png",
        "models/characters/knight_3/cav_textures/weapons/sword_long_Flattened_Diffuse_COLOR.jpg",
    };
    const char *const shield[]={
        "models/characters/knight_3/cav_textures/weapons/Crest_shield_Crest_shield_BaseColor.png",
    };
    ufbx_string_copy(node_name,sizeof(node_name),node->name);
    if(path_contains_ci(node_name,"sword")||path_contains_ci(node_name,"zweihander")) return try_texture_candidates(sword,sizeof(sword)/sizeof(sword[0]));
    if(path_contains_ci(node_name,"shield")||path_contains_ci(node_name,"crest")) return try_texture_candidates(shield,sizeof(shield)/sizeof(shield[0]));
    if(path_contains_ci(node_name,"belt")) return try_texture_candidates(belts,sizeof(belts)/sizeof(belts[0]));
    if(path_contains_ci(node_name,"leather")) return try_texture_candidates(leather,sizeof(leather)/sizeof(leather[0]));
    if(path_contains_ci(node_name,"chain")) return try_texture_candidates(chainmail,sizeof(chainmail)/sizeof(chainmail[0]));
    return try_texture_candidates(plates,sizeof(plates)/sizeof(plates[0]));
}

static bool is_running_paladin_asset(const char *path, ufbx_material *mat){
    char mat_name[256]={0};
    const char *base=path_basename_ptr(path?path:"");
    if(mat) ufbx_string_to_cstr(mat_name,sizeof(mat_name),mat->name);
    return path_contains_ci(base,"Running.fbx")||path_contains_ci(mat_name,"Paladin_MAT");
}

static GLuint get_running_paladin_albedo_override(void){
    const char *const cands[]={
        "palidin/textures/Paladin_diffuse.png",
        "palidin/Paladin_diffuse.png",
    };
    return try_texture_candidates(cands,sizeof(cands)/sizeof(cands[0]));
}

static GLuint get_running_paladin_normal_override(void){
    const char *const cands[]={
        "palidin/textures/Paladin_normal.png",
        "palidin/Paladin_normal.png",
    };
    return try_texture_candidates(cands,sizeof(cands)/sizeof(cands[0]));
}

static GLuint get_running_paladin_spec_override(void){
    const char *const cands[]={
        "palidin/textures/Paladin_specular.png",
        "palidin/Paladin_specular.png",
    };
    for(size_t i=0;i<sizeof(cands)/sizeof(cands[0]);i++){
        GLuint id=texture_file_try_candidates_mode(cands[i],false);
        if(id)return id;
    }
    return 0;
}

static GLuint get_character_override_tex(ufbx_node *node,ufbx_material *mat,const char *path){
    char node_name[256]={0},mat_name[256]={0},base_name[256]={0};
    const char *base=path_basename_ptr(path);
    if(node)ufbx_string_to_cstr(node_name,sizeof(node_name),node->name);
    if(mat) ufbx_string_to_cstr(mat_name,sizeof(mat_name),mat->name);
    snprintf(base_name,sizeof(base_name),"%s",base);

    if(path_contains_ci(base_name,"npc.fbx")){
        const char *const npc_tex[]={"knight-tex.jpg"};
        return try_texture_candidates(npc_tex,sizeof(npc_tex)/sizeof(npc_tex[0]));
    }

    if(is_running_paladin_asset(path,mat)){
        return get_running_paladin_albedo_override();
    }

    if(is_direct_knight_asset(path)&&!is_knight_copy_asset(path)){
        const char *const sword[]={"textures/ZWEIHANDER_ZWEIHANDER_BaseColor.png","models/characters/knight_3/cav_textures/weapons/sword_long_Flattened_Diffuse_COLOR.jpg"};
        const char *const belts[]={"textures/belts_belts_BaseColor.png","models/characters/knight_3/textures(4)/textures/belts_belts_BaseColor.png"};
        const char *const leather[]={"textures/knight_leather_BaseColor.png","models/characters/knight_3/textures(4)/textures/knight_leather_BaseColor.png"};
        const char *const chainmail[]={"textures/knight_chainmail_BaseColor.png","models/characters/knight_3/textures(4)/textures/knight_chainmail_BaseColor.png"};
        const char *const plates[]={"textures/knight_plates_BaseColor.png","models/characters/knight_3/textures(4)/textures/knight_plates_BaseColor.png"};
        if(path_contains_ci(node_name,"sword")||path_contains_ci(node_name,"zweihander")||path_contains_ci(mat_name,"zweihander")) return try_texture_candidates(sword,sizeof(sword)/sizeof(sword[0]));
        if(path_contains_ci(node_name,"belt")||path_contains_ci(mat_name,"belt")) return try_texture_candidates(belts,sizeof(belts)/sizeof(belts[0]));
        if(path_contains_ci(node_name,"leather")||path_contains_ci(mat_name,"leather")) return try_texture_candidates(leather,sizeof(leather)/sizeof(leather[0]));
        if(path_contains_ci(node_name,"chain")||path_contains_ci(mat_name,"chain")||path_contains_ci(mat_name,"head")) return try_texture_candidates(chainmail,sizeof(chainmail)/sizeof(chainmail[0]));
        return try_texture_candidates(plates,sizeof(plates)/sizeof(plates[0]));
    }

    if(path_contains_ci(base_name,"dwarf_")){
        char dwarf_id[64]="";
        if(path_contains_ci(base_name,"dwarf_broddi")) snprintf(dwarf_id,sizeof(dwarf_id),"Dwarf_Broddi");
        else if(path_contains_ci(base_name,"dwarf_king")) snprintf(dwarf_id,sizeof(dwarf_id),"Dwarf_King");
        else if(path_contains_ci(base_name,"dwarf_agvid")) snprintf(dwarf_id,sizeof(dwarf_id),"Dwarf_Agvid");
        else if(path_contains_ci(base_name,"dwarf_bedvar")) snprintf(dwarf_id,sizeof(dwarf_id),"Dwarf_Bedvar");
        if(dwarf_id[0]){
            const char *slot="Body";
            if(path_contains_ci(node_name,"weapon")||path_contains_ci(node_name,"forge")||path_contains_ci(mat_name,"weapon")) slot="Weapon";
            else if(path_contains_ci(node_name,"details")||path_contains_ci(mat_name,"details")) slot="Details";
            else if(path_contains_ci(node_name,"beard")||path_contains_ci(node_name,"mustache")||path_contains_ci(mat_name,"beard")) slot="Beard";
            else if(path_contains_ci(node_name,"head")||path_contains_ci(node_name,"hair")||path_contains_ci(node_name,"eyes")||
                    path_contains_ci(node_name,"eyebrow")||path_contains_ci(node_name,"jaw")||path_contains_ci(mat_name,"head")||
                    path_contains_ci(mat_name,"hair")) slot="Head";
            char tex_name[160];
            char tex_path1[256],tex_path2[256];
            snprintf(tex_name,sizeof(tex_name),"%s_%s_Albedo.tga",dwarf_id,slot);
            snprintf(tex_path1,sizeof(tex_path1),"textures/%s",tex_name);
            snprintf(tex_path2,sizeof(tex_path2),"models/characters/dawrves/textures/%s",tex_name);
            const char *cands[]={tex_path1,tex_path2};
            return try_texture_candidates(cands,sizeof(cands)/sizeof(cands[0]));
        }
    }
    return 0;
}

static bool resolve_mesh_texture_debug_path(ufbx_mesh *mesh,ufbx_node *node,const char *path,size_t material_index,char *resolved,size_t resolved_cap){
    if(!resolved||resolved_cap==0)return false;
    resolved[0]=0;
    if(is_direct_knight_asset(path)){
        char node_name[256]={0};
        ufbx_string_copy(node_name,sizeof(node_name),node->name);
        const char *guess=
            (path_contains_ci(node_name,"sword")||path_contains_ci(node_name,"zweihander")) ? "models/characters/knight_3/textures(4)/textures/ZWEIHANDER_ZWEIHANDER_BaseColor.png" :
            (path_contains_ci(node_name,"shield")||path_contains_ci(node_name,"crest")) ? "models/characters/knight_3/cav_textures/weapons/Crest_shield_Crest_shield_BaseColor.png" :
            path_contains_ci(node_name,"belt") ? "models/characters/knight_3/textures(4)/textures/belts_belts_BaseColor.png" :
            path_contains_ci(node_name,"leather") ? "textures/knight_leather_BaseColor.png" :
            path_contains_ci(node_name,"chain") ? "textures/knight_chainmail_BaseColor.png" :
            "textures/knight_plates_BaseColor.png";
        if(texture_path_pick_candidate(resolved,resolved_cap,guess)) return true;
    }
    if(mesh&&mesh->materials.count>0){
        ufbx_material *mat=mesh_material_at(mesh,node,material_index);
        if(mat){
            ufbx_texture *tex=mat->pbr.base_color.texture;if(!tex)tex=mat->fbx.diffuse_color.texture;
            if(tex){
                if(tex->relative_filename.length>0){
                    char full[1024];
                    texture_path_resolve(full,sizeof(full),path,tex->relative_filename);
                    if(texture_path_pick_candidate(resolved,resolved_cap,full)) return true;
                }else if(tex->content.size>4){
                    snprintf(resolved,resolved_cap,"<embedded>");
                    return true;
                }
            }
        }
    }
    return false;
}

static void debug_print_mesh_binding(ufbx_mesh *mesh,ufbx_node *node,const char *path,const ufbx_vertex_vec2 *uv_stream){
    if(!(path_contains_ci(path,"knight")||path_contains_ci(path,"platemail")||path_contains_ci(path,"character")))return;
    double umin=1e30,vmin=1e30,umax=-1e30,vmax=-1e30;
    if(uv_stream&&uv_stream->exists){
        for(size_t ix=0;ix<mesh->num_indices;ix++){
            ufbx_vec2 uv=ufbx_get_vertex_vec2(uv_stream,ix);
            if(uv.x<umin)umin=uv.x; if(uv.y<vmin)vmin=uv.y;
            if(uv.x>umax)umax=uv.x; if(uv.y>vmax)vmax=uv.y;
        }
    }else{
        umin=vmin=umax=vmax=0.0;
    }
    char resolved[1024]={0};
    const char *chosen="<none>";
    int ch=0;
    bool alpha=false;
    if(resolve_mesh_texture_debug_path(mesh,node,path,0,resolved,sizeof(resolved))) chosen=resolved;
    if(chosen[0]&&chosen[0]!='<'){
        int w=0,h=0;
        if(stbi_info(chosen,&w,&h,&ch)) alpha=(ch==4);
    }
    printf("[MeshDebug] model='%s' node='%s' uv='%s' uv_range=[%.3f..%.3f, %.3f..%.3f] tex='%s' ch=%d alpha=%s mats=%zu uv_sets=%zu\n",
           path,node->name.data,mesh_preferred_uv_name(mesh),umin,umax,vmin,vmax,chosen,ch,alpha?"yes":"no",mesh->materials.count,mesh->uv_sets.count);
}

/* AnimMap now always maps to fixed indices 0-9 */
static void animmap_init(AnimMap *am, ufbx_scene *sc){
    (void)sc; /* no longer needed — CharModel handles resolution */
    am->idle=ANIM_IDLE; am->walk=ANIM_WALK; am->run=ANIM_RUN;
    am->attack=ANIM_ATTACK; am->attack2=ANIM_ATTACK2; am->jump=ANIM_JUMP;
    am->death=ANIM_DEATH; am->roll=ANIM_ROLL; am->block=ANIM_BLOCK;
    am->hit=ANIM_HIT;
}


static GLuint get_mesh_tex(ufbx_mesh *mesh,ufbx_node *node,const char *path,size_t material_index){
    ufbx_material *mat=mesh_material_at(mesh,node,material_index);
    if(is_running_paladin_asset(path,mat)){
        GLuint id=get_running_paladin_albedo_override();
        if(id)return id;
        return 0;
    }
    if(mat){
        ufbx_texture *tex=mat->pbr.base_color.texture;if(!tex)tex=mat->fbx.diffuse_color.texture;
        if(tex){
            if(tex->content.size>4){GLuint id=GLH_texture_mem_srgb(tex->content.data,(int)tex->content.size);if(id)return id;}
            if(tex->relative_filename.length>0){
                char full[1024];
                texture_path_resolve(full,sizeof(full),path,tex->relative_filename);
                GLuint id=texture_file_try_candidates(full);if(id)return id;
            }
        }
    }
    return get_character_override_tex(node,mat,path);
}

static GLuint load_texture_from_map(const ufbx_material_map *map,const char *path,bool srgb){
    if(path&&path_contains_ci(path,"Running.fbx")){
        return 0;
    }
    if(!map||!map->texture_enabled||!map->texture)return 0;
    ufbx_texture *tex=map->texture;
    if(tex->content.size>4){
        return srgb ? GLH_texture_mem_srgb(tex->content.data,(int)tex->content.size)
                    : GLH_texture_mem(tex->content.data,(int)tex->content.size);
    }
    if(tex->relative_filename.length>0){
        char full[1024];
        texture_path_resolve(full,sizeof(full),path,tex->relative_filename);
        return texture_file_try_candidates_mode(full,srgb);
    }
    return 0;
}

static void gpu_mesh_apply_pbr(GpuMesh *gm,ufbx_node *node,ufbx_material *mat,const char *path){
    const char *node_name=node&&node->name.data?node->name.data:"";
    const char *mat_name=mat&&mat->name.data?mat->name.data:"";
    gm->mat_color[0]=0.75f;gm->mat_color[1]=0.72f;gm->mat_color[2]=0.68f;gm->mat_color[3]=1.0f;
    gm->emissive[0]=gm->emissive[1]=gm->emissive[2]=0.0f;
    gm->metallic=0.0f;
    gm->roughness=0.85f;
    gm->alpha_cutoff=0.33f;
    gm->has_pbr=false;
    gm->tint_region=scene_guess_tint_region(node_name,mat_name,path);
    if(!mat)return;

    gm->normal_tex=load_texture_from_map(&mat->pbr.normal_map,path,false);
    gm->mr_tex=load_texture_from_map(&mat->pbr.roughness,path,false);
    if(!gm->mr_tex)gm->mr_tex=load_texture_from_map(&mat->pbr.metalness,path,false);
    gm->emissive_tex=load_texture_from_map(&mat->pbr.emission_color,path,true);
    if(is_running_paladin_asset(path,mat)){
        gm->normal_tex=0;
        gm->mr_tex=0;
        gm->emissive_tex=0;
        gm->roughness=0.82f;
        gm->metallic=0.02f;
        gm->has_pbr=false;
    }

    /* RGB base_color_factor from the model is intentionally ignored — source assets
       frequently ship with incorrect tints (e.g. pure red) that override the texture.
       The texture provides all color; only alpha is kept for alpha-cutout geometry. */
    gm->mat_color[0]=1.0f; gm->mat_color[1]=1.0f; gm->mat_color[2]=1.0f;
    if(mat->pbr.base_color.value_components>=4){
        gm->mat_color[3]=(float)mat->pbr.base_color.value_vec4.w;
    }
    if(mat->pbr.emission_color.value_components>=3){
        gm->emissive[0]=(float)mat->pbr.emission_color.value_vec3.x;
        gm->emissive[1]=(float)mat->pbr.emission_color.value_vec3.y;
        gm->emissive[2]=(float)mat->pbr.emission_color.value_vec3.z;
    }
    if(mat->pbr.metalness.has_value)gm->metallic=clampf((float)mat->pbr.metalness.value_real,0.0f,1.0f);
    if(mat->pbr.roughness.has_value)gm->roughness=clampf((float)mat->pbr.roughness.value_real,0.04f,1.0f);
    if(mat->pbr.opacity.has_value){
        gm->mat_color[3]=clampf((float)mat->pbr.opacity.value_real,0.0f,1.0f);
    }
    gm->has_pbr=(gm->normal_tex||gm->mr_tex||gm->emissive_tex||mat->pbr.base_color.has_value||mat->pbr.metalness.has_value||mat->pbr.roughness.has_value);
}

static Mat4 mat4_from_cgltf(const cgltf_float *m16){
    Mat4 m=mat4_identity();
    if(!m16)return m;
    for(int i=0;i<16;i++)m.m[i]=(float)m16[i];
    return m;
}

static Quat quat_identity(void){return (Quat){0,0,0,1};}
static Quat quat_norm(Quat q){float l=sqrtf(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);if(l<1e-8f)return quat_identity();float il=1.0f/l;return (Quat){q.x*il,q.y*il,q.z*il,q.w*il};}
static Quat quat_nlerp(Quat a,Quat b,float t){
    float dot=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
    if(dot<0.0f){b.x=-b.x;b.y=-b.y;b.z=-b.z;b.w=-b.w;}
    Quat q={a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};
    return quat_norm(q);
}
static Mat4 mat4_from_quat(Quat q){
    q=quat_norm(q);
    float xx=q.x*q.x,yy=q.y*q.y,zz=q.z*q.z;
    float xy=q.x*q.y,xz=q.x*q.z,yz=q.y*q.z;
    float wx=q.w*q.x,wy=q.w*q.y,wz=q.w*q.z;
    Mat4 m=mat4_identity();
    m.m[0]=1.0f-2.0f*(yy+zz); m.m[4]=2.0f*(xy-wz);     m.m[8]=2.0f*(xz+wy);
    m.m[1]=2.0f*(xy+wz);     m.m[5]=1.0f-2.0f*(xx+zz); m.m[9]=2.0f*(yz-wx);
    m.m[2]=2.0f*(xz-wy);     m.m[6]=2.0f*(yz+wx);     m.m[10]=1.0f-2.0f*(xx+yy);
    return m;
}
static Mat4 mat4_from_trs(Vec3 t,Quat r,Vec3 s){
    Mat4 m=mat4_from_quat(r);
    m.m[0]*=s.x;m.m[1]*=s.x;m.m[2]*=s.x;
    m.m[4]*=s.y;m.m[5]*=s.y;m.m[6]*=s.y;
    m.m[8]*=s.z;m.m[9]*=s.z;m.m[10]*=s.z;
    m.m[12]=t.x;m.m[13]=t.y;m.m[14]=t.z;
    return m;
}
static GltfNodePose gltf_node_bind_pose(const cgltf_node *node){
    GltfNodePose p={0};
    p.t=(Vec3){0,0,0};
    p.s=(Vec3){1,1,1};
    p.r=quat_identity();
    if(node->has_translation)p.t=(Vec3){(float)node->translation[0],(float)node->translation[1],(float)node->translation[2]};
    if(node->has_scale)p.s=(Vec3){(float)node->scale[0],(float)node->scale[1],(float)node->scale[2]};
    if(node->has_rotation)p.r=(Quat){(float)node->rotation[0],(float)node->rotation[1],(float)node->rotation[2],(float)node->rotation[3]};
    if(node->has_matrix){
        cgltf_float m[16]; for(int i=0;i<16;i++)m[i]=node->matrix[i];
        p.t=(Vec3){(float)m[12],(float)m[13],(float)m[14]};
    }
    return p;
}
static int gltf_node_index(const Scene *s,const cgltf_node *node){
    if(!s||!s->gltf||!node)return -1;
    return (int)(node-s->gltf->nodes);
}
static float gltf_anim_duration(const cgltf_animation *anim){
    float end=0.0f;
    if(!anim)return 0.0f;
    for(size_t ci=0;ci<anim->channels_count;ci++){
        cgltf_animation_sampler *sam=anim->channels[ci].sampler;
        if(!sam||!sam->input||sam->input->count==0)continue;
        cgltf_float v=0.0f;
        cgltf_accessor_read_float(sam->input,sam->input->count-1,&v,1);
        if(v>end)end=(float)v;
    }
    return end;
}
static void gltf_sample_channel_pose(const cgltf_animation_channel *ch,float t,GltfNodePose *pose){
    if(!ch||!ch->sampler||!ch->sampler->input||!ch->sampler->output||!pose)return;
    cgltf_accessor *tin=ch->sampler->input,*tout=ch->sampler->output;
    if(tin->count==0||tout->count==0)return;
    size_t count=tin->count;
    size_t i0=0,i1=0;
    cgltf_float tt=0.0f;
    cgltf_accessor_read_float(tin,0,&tt,1);
    if(t<=tt){i0=i1=0;}
    else{
        i0=count-1;i1=count-1;
        for(size_t i=1;i<count;i++){
            cgltf_float cur=0.0f;
            cgltf_accessor_read_float(tin,i,&cur,1);
            if(t<=cur){i0=i-1;i1=i;break;}
        }
    }
    cgltf_float t0=0.0f,t1=0.0f;
    cgltf_accessor_read_float(tin,i0,&t0,1);
    cgltf_accessor_read_float(tin,i1,&t1,1);
    float a=(i0==i1||ch->sampler->interpolation==cgltf_interpolation_type_step||fabsf((float)(t1-t0))<1e-6f)?0.0f:clampf((t-(float)t0)/((float)t1-(float)t0),0.0f,1.0f);
    if(ch->sampler->interpolation==cgltf_interpolation_type_cubic_spline)a=0.0f;
    if(ch->target_path==cgltf_animation_path_type_translation||ch->target_path==cgltf_animation_path_type_scale){
        cgltf_float va[3]={0},vb[3]={0};
        cgltf_accessor_read_float(tout,i0,va,3);cgltf_accessor_read_float(tout,i1,vb,3);
        Vec3 out={(float)(va[0]+(vb[0]-va[0])*a),(float)(va[1]+(vb[1]-va[1])*a),(float)(va[2]+(vb[2]-va[2])*a)};
        if(ch->target_path==cgltf_animation_path_type_translation)pose->t=out; else pose->s=out;
    }else if(ch->target_path==cgltf_animation_path_type_rotation){
        cgltf_float va[4]={0,0,0,1},vb[4]={0,0,0,1};
        cgltf_accessor_read_float(tout,i0,va,4);cgltf_accessor_read_float(tout,i1,vb,4);
        pose->r=quat_nlerp((Quat){(float)va[0],(float)va[1],(float)va[2],(float)va[3]},(Quat){(float)vb[0],(float)vb[1],(float)vb[2],(float)vb[3]},a);
    }
}
static Mat4 gltf_eval_world_recursive(Scene *s,int node_index){
    if(node_index<0||node_index>=s->gltf_node_count)return mat4_identity();
    if(s->gltf_eval_mark[node_index]==2)return s->gltf_eval_world[node_index];
    if(s->gltf_eval_mark[node_index]==1)return s->gltf_eval_world[node_index];
    s->gltf_eval_mark[node_index]=1;
    cgltf_node *node=&s->gltf->nodes[node_index];
    Mat4 local=mat4_from_trs(s->gltf_eval_pose[node_index].t,s->gltf_eval_pose[node_index].r,s->gltf_eval_pose[node_index].s);
    if(node->parent){
        int parent_index=gltf_node_index(s,node->parent);
        s->gltf_eval_world[node_index]=mat4_mul(gltf_eval_world_recursive(s,parent_index),local);
    }else{
        s->gltf_eval_world[node_index]=local;
    }
    s->gltf_eval_mark[node_index]=2;
    return s->gltf_eval_world[node_index];
}
static void scene_gltf_evaluate(Scene *s,int anim_index,double at){
    if(!s||s->format!=SCENE_FORMAT_GLTF||!s->gltf||s->gltf_node_count<=0)return;
    for(int i=0;i<s->gltf_node_count;i++)s->gltf_eval_pose[i]=s->gltf_bind_pose[i];
    if(anim_index>=0&&anim_index<(int)s->gltf->animations_count){
        cgltf_animation *anim=&s->gltf->animations[anim_index];
        float dur=gltf_anim_duration(anim);
        float t=dur>0.0f?fmodf((float)at,dur):0.0f;
        for(size_t ci=0;ci<anim->channels_count;ci++){
            cgltf_animation_channel *ch=&anim->channels[ci];
            if(!ch->target_node||ch->target_path==cgltf_animation_path_type_weights)continue;
            int node_index=gltf_node_index(s,ch->target_node);
            if(node_index<0||node_index>=s->gltf_node_count)continue;
            gltf_sample_channel_pose(ch,t,&s->gltf_eval_pose[node_index]);
        }
    }
    memset(s->gltf_eval_mark,0,(size_t)s->gltf_node_count*sizeof(*s->gltf_eval_mark));
    for(int i=0;i<s->gltf_node_count;i++)gltf_eval_world_recursive(s,i);
}
/* True if the upper-left 3x3 of m contains a rotation (not just scale/identity).
   Normalizes each column and measures deviation from the axis-aligned basis. */
static bool mat4_has_rotation(Mat4 m){
    float l0=sqrtf(m.m[0]*m.m[0]+m.m[1]*m.m[1]+m.m[2]*m.m[2]);
    float l1=sqrtf(m.m[4]*m.m[4]+m.m[5]*m.m[5]+m.m[6]*m.m[6]);
    float l2=sqrtf(m.m[8]*m.m[8]+m.m[9]*m.m[9]+m.m[10]*m.m[10]);
    if(l0<1e-6f||l1<1e-6f||l2<1e-6f)return false;
    float dev=fabsf(m.m[0]/l0-1.0f)+fabsf(m.m[1]/l0)+fabsf(m.m[2]/l0)
             +fabsf(m.m[4]/l1)+fabsf(m.m[5]/l1-1.0f)+fabsf(m.m[6]/l1)
             +fabsf(m.m[8]/l2)+fabsf(m.m[9]/l2)+fabsf(m.m[10]/l2-1.0f);
    return dev>0.1f;
}

static void compute_bones_gltf(Mat4 *out,int nb,const GpuMesh *gm,const Scene *sc){
    for(int i=0;i<nb;i++)out[i]=mat4_identity();
    if(!sc||sc->format!=SCENE_FORMAT_GLTF||!sc->gltf_skins)return;
    int skin_index=gm->cluster_map[0];
    if(skin_index<0||skin_index>=sc->gltf_skin_count)return;
    const GltfSkinInfo *skin=&sc->gltf_skins[skin_index];
    /* The draw matrix contains the actor transform only. Joint-world times
       inverse-bind already includes the mesh's global transform, as required
       by glTF skinning. Applying mesh inverse here loses root scale. */
    int count=skin->joint_count<nb?skin->joint_count:nb;
    for(int i=0;i<count;i++){
        int node_index=skin->joint_nodes[i];
        if(node_index<0||node_index>=sc->gltf_node_count)continue;
        out[i]=mat4_mul(sc->gltf_eval_world[node_index],skin->inverse_bind[i]);
    }
}

static void modelc_sample_channel_pose(const ModelcAnimChannel *ch,float t,GltfNodePose *pose){
    if(!ch||!pose||!ch->times||!ch->values||ch->key_count==0)return;
    size_t i0=0,i1=0,count=ch->key_count;
    if(t<=ch->times[0]){ i0=i1=0; }
    else{
        i0=count-1;i1=count-1;
        for(size_t i=1;i<count;i++) if(t<=ch->times[i]){ i0=i-1; i1=i; break; }
    }
    {
        float t0=ch->times[i0],t1=ch->times[i1];
        float a=(i0==i1||ch->interpolation==1||fabsf(t1-t0)<1e-6f)?0.0f:clampf((t-t0)/(t1-t0),0.0f,1.0f);
        if(ch->interpolation==2)a=0.0f;
        if(ch->path==MODELC_ANIM_TRANSLATION||ch->path==MODELC_ANIM_SCALE){
            Vec3 out={
                ch->values[i0][0]+(ch->values[i1][0]-ch->values[i0][0])*a,
                ch->values[i0][1]+(ch->values[i1][1]-ch->values[i0][1])*a,
                ch->values[i0][2]+(ch->values[i1][2]-ch->values[i0][2])*a
            };
            if(ch->path==MODELC_ANIM_TRANSLATION)pose->t=out; else pose->s=out;
        }else if(ch->path==MODELC_ANIM_ROTATION){
            pose->r=quat_nlerp((Quat){ch->values[i0][0],ch->values[i0][1],ch->values[i0][2],ch->values[i0][3]},
                               (Quat){ch->values[i1][0],ch->values[i1][1],ch->values[i1][2],ch->values[i1][3]},a);
        }
    }
}

static Mat4 modelc_eval_world_recursive(Scene *s,int node_index){
    if(node_index<0||node_index>=s->modelc_node_count)return mat4_identity();
    if(s->modelc_eval_mark[node_index]==2)return s->modelc_eval_world[node_index];
    if(s->modelc_eval_mark[node_index]==1)return s->modelc_eval_world[node_index];
    s->modelc_eval_mark[node_index]=1;
    {
        Mat4 local=mat4_from_trs(s->modelc_eval_pose[node_index].t,s->modelc_eval_pose[node_index].r,s->modelc_eval_pose[node_index].s);
        int parent=s->modelc_parent_index?s->modelc_parent_index[node_index]:-1;
        if(parent>=0&&parent<s->modelc_node_count)s->modelc_eval_world[node_index]=mat4_mul(modelc_eval_world_recursive(s,parent),local);
        else s->modelc_eval_world[node_index]=local;
    }
    s->modelc_eval_mark[node_index]=2;
    return s->modelc_eval_world[node_index];
}

static void scene_modelc_evaluate(Scene *s,int anim_index,double at){
    if(!s||s->format!=SCENE_FORMAT_MODELC||s->modelc_node_count<=0)return;
    for(int i=0;i<s->modelc_node_count;i++)s->modelc_eval_pose[i]=s->modelc_bind_pose[i];
    if(anim_index>=0&&anim_index<s->modelc_anim_count){
        ModelcAnimation *anim=&s->modelc_anims[anim_index];
        float dur=anim->duration;
        float t=dur>0.0f?fmodf((float)at,dur):0.0f;
        for(uint32_t ci=0;ci<anim->channel_count;ci++){
            ModelcAnimChannel *ch=&anim->channels[ci];
            if(ch->node_index>=(uint32_t)s->modelc_node_count)continue;
            modelc_sample_channel_pose(ch,t,&s->modelc_eval_pose[ch->node_index]);
        }
    }
    memset(s->modelc_eval_mark,0,(size_t)s->modelc_node_count*sizeof(*s->modelc_eval_mark));
    for(int i=0;i<s->modelc_node_count;i++)modelc_eval_world_recursive(s,i);
}

static void compute_bones_modelc(Mat4 *out,int nb,const GpuMesh *gm,const Scene *sc){
    for(int i=0;i<nb;i++)out[i]=mat4_identity();
    if(!sc||sc->format!=SCENE_FORMAT_MODELC||!sc->modelc_skins)return;
    int skin_index=sc->modelc_mesh_skin[gm-sc->meshes];
    if(skin_index<0||skin_index>=sc->modelc_skin_count)return;
    const GltfSkinInfo *skin=&sc->modelc_skins[skin_index];
    {
        int count=skin->joint_count<nb?skin->joint_count:nb;
        for(int i=0;i<count;i++){
            int node_index=skin->joint_nodes[i];
            if(node_index<0||node_index>=sc->modelc_node_count)continue;
            out[i]=mat4_mul(sc->modelc_eval_world[node_index],skin->inverse_bind[i]);
        }
    }
}

static const cgltf_attribute *gltf_find_attribute(const cgltf_primitive *prim,cgltf_attribute_type type,int index){
    if(!prim)return NULL;
    for(size_t i=0;i<prim->attributes_count;i++){
        const cgltf_attribute *attr=&prim->attributes[i];
        if(attr->type==type&&attr->index==index&&attr->data)return attr;
    }
    return NULL;
}

static void model_dir_from_path(char *dst,size_t cap,const char *path){
    if(!dst||cap==0)return;
    dst[0]=0;
    if(!path||!path[0])return;
    snprintf(dst,cap,"%s",path);
    path_normalize_slashes(dst);
    char *slash=strrchr(dst,'/');
    if(slash)slash[1]=0;
    else dst[0]=0;
}

static bool gltf_texture_info_ok_memory(const void *data,int size,const char *label){
    if(!data||size<=4)return false;
    int w=0,h=0,ch=0;
    if(!stbi_info_from_memory((const stbi_uc*)data,size,&w,&h,&ch))return true;
    if(w<=0||h<=0)return false;
    if(w>4096||h>4096||(int64_t)w*(int64_t)h>16777216LL){
        fprintf(stderr,"[glTF] Skipping oversized embedded texture %s (%dx%d)\n",label?label:"<embedded>",w,h);
        return false;
    }
    return true;
}

static bool gltf_texture_info_ok_file(const char *path){
    if(!path||!path[0])return false;
    int w=0,h=0,ch=0;
    if(!stbi_info(path,&w,&h,&ch))return true;
    if(w<=0||h<=0)return false;
    if(w>4096||h>4096||(int64_t)w*(int64_t)h>16777216LL){
        fprintf(stderr,"[glTF] Skipping oversized texture file %s (%dx%d)\n",path,w,h);
        return false;
    }
    return true;
}

static const cgltf_image *gltf_texture_image_ptr(const cgltf_texture *tex){
    if(!tex)return NULL;
    if(tex->image)return tex->image;
    if(tex->has_basisu&&tex->basisu_image)return tex->basisu_image;
    if(tex->has_webp&&tex->webp_image)return tex->webp_image;
    return NULL;
}

static unsigned gltf_texture_upload_shared(const void *data,size_t size,const TextureCachePolicy *policy){
    return policy->srgb?GLH_texture_mem_srgb(data,(int)size):GLH_texture_mem(data,(int)size);
}
static void gltf_texture_destroy_shared(unsigned texture){
    GLuint id=(GLuint)texture;
    glDeleteTextures(1,&id);
}
static GLuint gltf_texture_from_embedded_shared(const void *data,int size,bool srgb){
    /* Preserve the existing effective GLH upload/sampler policy exactly. The
       loader currently uses these settings for every embedded glTF image. */
    const TextureCachePolicy policy={.srgb=srgb,.min_filter=GL_LINEAR_MIPMAP_LINEAR,
        .mag_filter=GL_LINEAR,.wrap_s=GL_CLAMP_TO_EDGE,.wrap_t=GL_CLAMP_TO_EDGE,.mip_policy=1};
    return (GLuint)texture_cache_acquire(data,(size_t)size,&policy,
                                        gltf_texture_upload_shared,gltf_texture_destroy_shared);
}

static GLuint gltf_texture_from_image(const cgltf_image *image,const char *path,bool srgb){
    if(!image)return 0;
    if(image->buffer_view&&image->buffer_view->buffer&&image->buffer_view->buffer->data){
        const void *data=cgltf_buffer_view_data(image->buffer_view);
        int size=(int)image->buffer_view->size;
        if(data&&size>4&&gltf_texture_info_ok_memory(data,size,image->name))return gltf_texture_from_embedded_shared(data,size,srgb);
    }
    if(image->uri&&image->uri[0]){
        if(!strncmp(image->uri,"data:",5)){
            const char *base64=strstr(image->uri,"base64,");
            if(base64){
                void *decoded=NULL;
                cgltf_size encoded_size=(cgltf_size)strlen(base64+7);
                cgltf_size decoded_size=encoded_size*3/4+4;
                cgltf_options opts={0};
                if(cgltf_load_buffer_base64(&opts,encoded_size,base64+7,&decoded)==cgltf_result_success&&decoded){
                    GLuint id=0;
                    if(gltf_texture_info_ok_memory(decoded,(int)decoded_size,image->name))id=srgb?GLH_texture_mem_srgb(decoded,(int)decoded_size):GLH_texture_mem(decoded,(int)decoded_size);
                    free(decoded);
                    return id;
                }
            }
            return 0;
        }
        char full[1024],dir[1024];
        model_dir_from_path(dir,sizeof(dir),path);
        snprintf(full,sizeof(full),"%s%s",dir,image->uri);
        path_normalize_slashes(full);
        if(!gltf_texture_info_ok_file(full))return 0;
        return texture_file_try_candidates_mode(full,srgb);
    }
    return 0;
}

static GLuint gltf_texture_from_view_cached(Scene *s,const cgltf_texture_view *view,const char *path,bool srgb){
    if(!view||!view->texture)return 0;
    const cgltf_image *image=gltf_texture_image_ptr(view->texture);
    if(image&&s){
        for(int i=0;i<s->gltf_tex_cache_count;i++){
            GltfTexCacheEntry *entry=&s->gltf_tex_cache[i];
            if(entry->image==image&&entry->srgb==srgb){
                /* The scene cache borrows IDs; each returned mesh field owns
                   a separate managed reference, including aliases in a scene. */
                texture_cache_retain(entry->tex);
                return entry->tex;
            }
        }
    }
    GLuint id=gltf_texture_from_image(image,path,srgb);
    if(id&&image&&s&&s->gltf_tex_cache&&s->gltf_tex_cache_count<s->gltf_tex_cache_cap){
        GltfTexCacheEntry *entry=&s->gltf_tex_cache[s->gltf_tex_cache_count++];
        entry->image=image;
        entry->srgb=srgb;
        entry->tex=id;
    }
    return id;
}

static bool build_gpu_mesh_gltf(Scene *s,GpuMesh *gm,const cgltf_primitive *prim,const cgltf_node *node,int node_index,int skin_index,const char *path){
    const cgltf_attribute *pos_attr=gltf_find_attribute(prim,cgltf_attribute_type_position,0);
    if(!pos_attr||!pos_attr->data||pos_attr->data->count==0)return false;
    const cgltf_attribute *nrm_attr=gltf_find_attribute(prim,cgltf_attribute_type_normal,0);
    const cgltf_attribute *uv_attr =gltf_find_attribute(prim,cgltf_attribute_type_texcoord,0);
    const cgltf_attribute *tan_attr=gltf_find_attribute(prim,cgltf_attribute_type_tangent,0);
    const cgltf_attribute *jnt_attr=gltf_find_attribute(prim,cgltf_attribute_type_joints,0);
    const cgltf_attribute *wgt_attr=gltf_find_attribute(prim,cgltf_attribute_type_weights,0);
    size_t vcount=pos_attr->data->count;
    size_t icount=prim->indices?prim->indices->count:vcount;
    if(vcount==0||icount==0)return false;
    Vertex *verts=calloc(vcount,sizeof(Vertex));
    uint32_t *inds=malloc(icount*sizeof(uint32_t));
    if(!verts||!inds){free(verts);free(inds);return false;}
    for(size_t i=0;i<vcount;i++){
        cgltf_float fv[4]={0,0,0,1};
        cgltf_uint uv[4]={0};
        Vertex *v=&verts[i];
        cgltf_accessor_read_float(pos_attr->data,i,fv,3);v->pos[0]=(float)fv[0];v->pos[1]=(float)fv[1];v->pos[2]=(float)fv[2];
        if(nrm_attr&&cgltf_accessor_read_float(nrm_attr->data,i,fv,3)){v->normal[0]=(float)fv[0];v->normal[1]=(float)fv[1];v->normal[2]=(float)fv[2];}
        else v->normal[1]=1.0f;
        if(uv_attr&&cgltf_accessor_read_float(uv_attr->data,i,fv,2)){v->uv[0]=(float)fv[0];v->uv[1]=(float)fv[1];}
        if(tan_attr&&cgltf_accessor_read_float(tan_attr->data,i,fv,4)){
            v->tangent[0]=(float)fv[0];v->tangent[1]=(float)fv[1];v->tangent[2]=(float)fv[2];
            Vec3 nn={v->normal[0],v->normal[1],v->normal[2]};
            Vec3 tt={v->tangent[0],v->tangent[1],v->tangent[2]};
            Vec3 bb=vec3_scale(vec3_norm(vec3_cross(nn,tt)),fv[3]<0.0f?-1.0f:1.0f);
            v->bitangent[0]=bb.x;v->bitangent[1]=bb.y;v->bitangent[2]=bb.z;
        }else{
            Vec3 nn={v->normal[0],v->normal[1],v->normal[2]};
            Vec3 tt=fabsf(nn.y)<0.99f?(Vec3){0,1,0}:(Vec3){1,0,0};
            tt=vec3_norm(vec3_sub(tt,vec3_scale(nn,vec3_dot(tt,nn))));
            Vec3 bb=vec3_norm(vec3_cross(nn,tt));
            v->tangent[0]=tt.x;v->tangent[1]=tt.y;v->tangent[2]=tt.z;
            v->bitangent[0]=bb.x;v->bitangent[1]=bb.y;v->bitangent[2]=bb.z;
        }
        if(jnt_attr&&wgt_attr){
            cgltf_accessor_read_uint(jnt_attr->data,i,uv,4);
            cgltf_accessor_read_float(wgt_attr->data,i,fv,4);
            float tw=0.0f;
            for(int k=0;k<MAX_WEIGHTS;k++){
                v->bones[k]=(int32_t)uv[k];
                v->weights[k]=(float)fv[k];
                tw+=v->weights[k];
            }
            if(tw>0.001f)for(int k=0;k<MAX_WEIGHTS;k++)v->weights[k]/=tw;
        }
    }
    for(size_t i=0;i<icount;i++)inds[i]=prim->indices?(uint32_t)cgltf_accessor_read_index(prim->indices,i):(uint32_t)i;
    GLH_Attrib attrs[]={{0,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,pos)},{1,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,normal)},{2,2,GL_FLOAT,GL_FALSE,offsetof(Vertex,uv)},{3,4,GL_FLOAT,GL_FALSE,offsetof(Vertex,weights)},{4,4,GL_INT,0,offsetof(Vertex,bones)},{5,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,tangent)},{6,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,bitangent)}};
    gm->vao=GLH_mesh_full(verts,vcount*sizeof(Vertex),inds,icount*sizeof(uint32_t),sizeof(Vertex),attrs,7,
                          &gm->vbo,&gm->ebo);
    gm->index_count=(GLsizei)icount;
    cgltf_float world[16];
    cgltf_node_transform_world(node,world);
    gm->geometry_to_world=mat4_from_cgltf(world);
    gm->node_typed_id=(uint32_t)node_index;
    gm->cluster_map[0]=skin_index;
    gm->texture=0;
    gm->normal_tex=gm->mr_tex=gm->emissive_tex=0;
    gm->mat_color[0]=gm->mat_color[1]=gm->mat_color[2]=1.0f;gm->mat_color[3]=1.0f;
    gm->emissive[0]=gm->emissive[1]=gm->emissive[2]=0.0f;
    gm->metallic=1.0f;gm->roughness=1.0f;gm->alpha_cutoff=0.0f;gm->has_pbr=false;
    gm->tint_region=scene_guess_tint_region(node?node->name?node->name:"":"",prim->material&&prim->material->name?prim->material->name:"",path);
    if(prim->material){
        const cgltf_material *mat=prim->material;
        gm->texture=gltf_texture_from_view_cached(s,&mat->pbr_metallic_roughness.base_color_texture,path,true);
        gm->normal_tex=gltf_texture_from_view_cached(s,&mat->normal_texture,path,false);
        gm->mr_tex=gltf_texture_from_view_cached(s,&mat->pbr_metallic_roughness.metallic_roughness_texture,path,false);
        gm->emissive_tex=gltf_texture_from_view_cached(s,&mat->emissive_texture,path,true);
        if(mat->has_pbr_metallic_roughness){
            for(int c=0;c<3;c++) gm->mat_color[c]=(float)mat->pbr_metallic_roughness.base_color_factor[c];
            gm->mat_color[3]=(float)mat->pbr_metallic_roughness.base_color_factor[3];
            gm->metallic=(float)mat->pbr_metallic_roughness.metallic_factor;
            gm->roughness=clampf((float)mat->pbr_metallic_roughness.roughness_factor,0.04f,1.0f);
        }
        gm->emissive[0]=(float)mat->emissive_factor[0];
        gm->emissive[1]=(float)mat->emissive_factor[1];
        gm->emissive[2]=(float)mat->emissive_factor[2];
        if(mat->alpha_mode==cgltf_alpha_mode_mask)gm->alpha_cutoff=mat->alpha_cutoff>0.0f?(float)mat->alpha_cutoff:0.5f;
        else gm->alpha_cutoff=0.0f;
        gm->has_pbr=(gm->texture||gm->normal_tex||gm->mr_tex||gm->emissive_tex||mat->has_pbr_metallic_roughness);
        gm->double_sided=mat->double_sided?true:false;
    }
    if(jnt_attr&&wgt_attr){
        int max_bone=0;
        for(size_t i=0;i<vcount;i++)for(int k=0;k<MAX_WEIGHTS;k++)if(verts[i].weights[k]>0.0f&&verts[i].bones[k]>max_bone)max_bone=verts[i].bones[k];
        gm->num_gpu_bones=max_bone+1;
        if(gm->num_gpu_bones>MAX_BONES)gm->num_gpu_bones=MAX_BONES;
    }
    free(verts);
    free(inds);
    return true;
}

static bool scene_load_gltf(Scene *s,const char *path){
    cgltf_options opts={0};
    cgltf_result res=cgltf_parse_file(&opts,path,&s->gltf);
    if(res!=cgltf_result_success||!s->gltf){
        fprintf(stderr,"Load '%s': cgltf parse failed (%d)\n",path,(int)res);
        return false;
    }
    res=cgltf_load_buffers(&opts,s->gltf,path);
    if(res!=cgltf_result_success){
        fprintf(stderr,"Load '%s': cgltf buffers failed (%d)\n",path,(int)res);
        cgltf_free(s->gltf);s->gltf=NULL;
        return false;
    }
    s->format=SCENE_FORMAT_GLTF;
    s->gltf_node_count=(int)s->gltf->nodes_count;
    if(s->gltf_node_count>0){
        s->gltf_bind_pose=calloc((size_t)s->gltf_node_count,sizeof(*s->gltf_bind_pose));
        s->gltf_rest_world=calloc((size_t)s->gltf_node_count,sizeof(*s->gltf_rest_world));
        s->gltf_eval_pose=calloc((size_t)s->gltf_node_count,sizeof(*s->gltf_eval_pose));
        s->gltf_eval_world=calloc((size_t)s->gltf_node_count,sizeof(*s->gltf_eval_world));
        s->gltf_eval_mark=calloc((size_t)s->gltf_node_count,sizeof(*s->gltf_eval_mark));
        if(!s->gltf_bind_pose||!s->gltf_rest_world||!s->gltf_eval_pose||!s->gltf_eval_world||!s->gltf_eval_mark){
            fprintf(stderr,"Load '%s': glTF runtime alloc failed\n",path);
            return false;
        }
        for(int i=0;i<s->gltf_node_count;i++){
            s->gltf_bind_pose[i]=gltf_node_bind_pose(&s->gltf->nodes[i]);
            cgltf_float world[16];
            cgltf_node_transform_world(&s->gltf->nodes[i],world);
            s->gltf_rest_world[i]=mat4_from_cgltf(world);
            s->gltf_eval_pose[i]=s->gltf_bind_pose[i];
            s->gltf_eval_world[i]=s->gltf_rest_world[i];
        }
    }
    s->gltf_skin_count=(int)s->gltf->skins_count;
    if(s->gltf_skin_count>0){
        s->gltf_skins=calloc((size_t)s->gltf_skin_count,sizeof(*s->gltf_skins));
        if(!s->gltf_skins){
            fprintf(stderr,"Load '%s': glTF skin alloc failed\n",path);
            return false;
        }
        for(int si=0;si<s->gltf_skin_count;si++){
            cgltf_skin *skin=&s->gltf->skins[si];
            GltfSkinInfo *dst=&s->gltf_skins[si];
            dst->joint_count=(int)(skin->joints_count<(cgltf_size)MAX_BONES?skin->joints_count:(cgltf_size)MAX_BONES);
            for(int ji=0;ji<dst->joint_count;ji++){
                dst->joint_nodes[ji]=gltf_node_index(s,skin->joints[ji]);
                dst->inverse_bind[ji]=mat4_identity();
                if(skin->inverse_bind_matrices&&ji<(int)skin->inverse_bind_matrices->count){
                    cgltf_float m[16]={0};
                    if(cgltf_accessor_read_float(skin->inverse_bind_matrices,(cgltf_size)ji,m,16))dst->inverse_bind[ji]=mat4_from_cgltf(m);
                }
            }
        }
    }
    if(s->gltf->images_count>0){
        cgltf_size want=s->gltf->images_count*2;
        if(want>(cgltf_size)MAX_GLTF_TEX_CACHE)want=MAX_GLTF_TEX_CACHE;
        s->gltf_tex_cache=calloc((size_t)want,sizeof(*s->gltf_tex_cache));
        if(!s->gltf_tex_cache&&want>0){
            fprintf(stderr,"Load '%s': glTF texture cache alloc failed\n",path);
            return false;
        }
        s->gltf_tex_cache_cap=(int)want;
    }
    for(int i=0;i<MAX_MESHES;i++)s->gltf_mesh_skin[i]=-1;
    s->num_meshes=0;
    float xmin=1e9f,ymin=1e9f,zmin=1e9f,xmax=-1e9f,ymax=-1e9f,zmax=-1e9f;
    float ymin_local=1e9f,ymax_local=-1e9f;
    for(size_t ni=0;ni<s->gltf->nodes_count&&s->num_meshes<MAX_MESHES;ni++){
        cgltf_node *nd=&s->gltf->nodes[ni];
        if(!nd->mesh)continue;
        cgltf_float world[16];
        cgltf_node_transform_world(nd,world);
        Mat4 wm=mat4_from_cgltf(world);
        int node_index=(int)ni;
        int skin_index=-1;
        if(nd->skin)skin_index=(int)(nd->skin-s->gltf->skins);
        for(size_t pi=0;pi<nd->mesh->primitives_count&&s->num_meshes<MAX_MESHES;pi++){
            if(build_gpu_mesh_gltf(s,&s->meshes[s->num_meshes],&nd->mesh->primitives[pi],nd,node_index,skin_index,path)){
                s->gltf_mesh_skin[s->num_meshes]=skin_index;
                s->num_meshes++;
            }
        }
        Mat4 rest_bones[MAX_BONES];
        if(skin_index>=0){
            GltfSkinInfo *skin=&s->gltf_skins[skin_index];
            for(int j=0;j<skin->joint_count;j++) rest_bones[j]=mat4_mul(s->gltf_rest_world[skin->joint_nodes[j]],skin->inverse_bind[j]);
        }
        for(size_t pi=0;pi<nd->mesh->primitives_count;pi++){
            const cgltf_attribute *pos_attr=gltf_find_attribute(&nd->mesh->primitives[pi],cgltf_attribute_type_position,0);
            if(!pos_attr||!pos_attr->data)continue;
            const cgltf_attribute *joints=gltf_find_attribute(&nd->mesh->primitives[pi],cgltf_attribute_type_joints,0);
            const cgltf_attribute *weights=gltf_find_attribute(&nd->mesh->primitives[pi],cgltf_attribute_type_weights,0);
            for(size_t vi=0;vi<pos_attr->data->count;vi++){
                cgltf_float p[3]={0};
                if(!cgltf_accessor_read_float(pos_attr->data,vi,p,3))continue;
                float x,y,z;
                if(skin_index>=0&&joints&&weights){
                    cgltf_float js[4]={0},ws[4]={0};
                    cgltf_accessor_read_float(joints->data,vi,js,4);cgltf_accessor_read_float(weights->data,vi,ws,4);
                    x=y=z=0;
                    for(int c=0;c<4;c++){
                        int j=(int)js[c]; if(j<0||j>=s->gltf_skins[skin_index].joint_count) continue;
                        Mat4 m=rest_bones[j];
                        x+=ws[c]*(m.m[0]*p[0]+m.m[4]*p[1]+m.m[8]*p[2]+m.m[12]);
                        y+=ws[c]*(m.m[1]*p[0]+m.m[5]*p[1]+m.m[9]*p[2]+m.m[13]);
                        z+=ws[c]*(m.m[2]*p[0]+m.m[6]*p[1]+m.m[10]*p[2]+m.m[14]);
                    }
                }else{
                    x=wm.m[0]*p[0]+wm.m[4]*p[1]+wm.m[8]*p[2]+wm.m[12];
                    y=wm.m[1]*p[0]+wm.m[5]*p[1]+wm.m[9]*p[2]+wm.m[13];
                    z=wm.m[2]*p[0]+wm.m[6]*p[1]+wm.m[10]*p[2]+wm.m[14];
                }
                if(x<xmin)xmin=x;if(x>xmax)xmax=x;
                if(y<ymin)ymin=y;if(y>ymax)ymax=y;
                if(z<zmin)zmin=z;if(z>zmax)zmax=z;
                if(p[1]<ymin_local)ymin_local=p[1];
                if(p[1]>ymax_local)ymax_local=p[1];
            }
        }
    }
    if(s->num_meshes==0){cgltf_free(s->gltf);s->gltf=NULL;s->format=SCENE_FORMAT_NONE;return false;}
    s->cx=(xmin+xmax)*0.5f;s->cy=(ymin+ymax)*0.5f;s->cz=(zmin+zmax)*0.5f;
    s->ymin_raw=ymin_local;s->ymax_raw=ymax_local;
    float md=fmaxf(xmax-xmin,fmaxf(ymax-ymin,zmax-zmin));if(md<0.0001f)md=1.0f;
    s->model_scale=2.0f/md;s->ground_offset=-(ymin-s->cy)*s->model_scale;
    printf("[Scene] %s: %d glTF meshes\n",path,s->num_meshes);
    return true;
}

static bool build_gpu_mesh(GpuMesh *gm,ufbx_skin_deformer **skin_out,ufbx_mesh *mesh,ufbx_node *node,const char *path,size_t material_index){
    ufbx_skin_deformer *skin=mesh->skin_deformers.count>0?mesh->skin_deformers.data[0]:NULL;
    MeshNormals mesh_normals=mesh_normals_build(mesh);
    const ufbx_vertex_vec2 *uv_stream=mesh_preferred_uv(mesh);
    const ufbx_vertex_vec3 *tan_stream=mesh_preferred_tangent(mesh);
    const ufbx_vertex_vec3 *bitan_stream=mesh_preferred_bitangent(mesh);
    debug_print_mesh_binding(mesh,node,path,uv_stream);
    *skin_out=skin;
    int32_t c2g[MAX_BONES];memset(c2g,-1,sizeof(c2g));gm->num_gpu_bones=0;
    if(skin){size_t nc=skin->clusters.count<MAX_BONES?skin->clusters.count:MAX_BONES;for(size_t ci=0;ci<nc;ci++){c2g[ci]=(int32_t)ci;gm->cluster_map[ci]=(int32_t)ci;}gm->num_gpu_bones=(int32_t)nc;}
    size_t ntri=mesh->max_face_triangles*3;uint32_t *tri=malloc(ntri*sizeof(uint32_t));if(!tri){mesh_normals_free(&mesh_normals);return false;}
    Vertex *verts=NULL;uint32_t *inds=NULL;size_t vc=0,ic=0,vcap=4096,icap=4096;
    verts=malloc(vcap*sizeof(Vertex));inds=malloc(icap*sizeof(uint32_t));if(!verts||!inds){free(tri);mesh_normals_free(&mesh_normals);return false;}
    for(size_t fi=0;fi<mesh->num_faces;fi++){
        if(mesh->materials.count>1&&mesh->face_material.count==mesh->num_faces){
            uint32_t face_mat=mesh->face_material.data[fi];
            if(face_mat!=material_index)continue;
        }
        ufbx_face face=mesh->faces.data[fi];size_t nt=ufbx_triangulate_face(tri,ntri,mesh,face);
        for(size_t ti=0;ti<nt*3;ti++){
            uint32_t ix=tri[ti],vi=mesh->vertex_indices.data[ix];Vertex v={0};
            ufbx_vec3 p=ufbx_get_vertex_vec3(&mesh->vertex_position,ix);v.pos[0]=(float)p.x;v.pos[1]=(float)p.y;v.pos[2]=(float)p.z;
            ufbx_vec3 n=mesh_normal_get(&mesh_normals,mesh,ix);v.normal[0]=(float)n.x;v.normal[1]=(float)n.y;v.normal[2]=(float)n.z;
            if(uv_stream&&uv_stream->exists){ufbx_vec2 uv=ufbx_get_vertex_vec2(uv_stream,ix);v.uv[0]=(float)uv.x;v.uv[1]=(float)uv.y;}
            if(tan_stream&&tan_stream->exists){
                ufbx_vec3 t=ufbx_get_vertex_vec3(tan_stream,ix);
                v.tangent[0]=(float)t.x;v.tangent[1]=(float)t.y;v.tangent[2]=(float)t.z;
            }
            if(bitan_stream&&bitan_stream->exists){
                ufbx_vec3 b=ufbx_get_vertex_vec3(bitan_stream,ix);
                v.bitangent[0]=(float)b.x;v.bitangent[1]=(float)b.y;v.bitangent[2]=(float)b.z;
            }else{
                Vec3 nn={v.normal[0],v.normal[1],v.normal[2]};
                Vec3 tt={v.tangent[0],v.tangent[1],v.tangent[2]};
                if(fabsf(tt.x)+fabsf(tt.y)+fabsf(tt.z)<1e-5f){
                    tt=fabsf(nn.y)<0.99f?(Vec3){0,1,0}:(Vec3){1,0,0};
                    tt=vec3_norm(vec3_sub(tt,vec3_scale(nn,vec3_dot(tt,nn))));
                    v.tangent[0]=tt.x;v.tangent[1]=tt.y;v.tangent[2]=tt.z;
                }
                Vec3 bb=vec3_norm(vec3_cross(nn,tt));
                v.bitangent[0]=bb.x;v.bitangent[1]=bb.y;v.bitangent[2]=bb.z;
            }
            if(skin){
                /* Guard: skin->vertices may be smaller than mesh vertex count
                   if the FBX has helper/proxy geometry sharing the skin deformer */
                if(vi < skin->vertices.count){
                    ufbx_skin_vertex sv=skin->vertices.data[vi];
                    size_t nw=sv.num_weights<MAX_WEIGHTS?sv.num_weights:MAX_WEIGHTS;
                    float tw=0;
                    int out=0; /* pack valid weights densely into slots 0..3 */
                    for(size_t wi=0;wi<nw&&out<MAX_WEIGHTS;wi++){
                        ufbx_skin_weight sw=skin->weights.data[sv.weight_begin+wi];
                        if(sw.cluster_index>=(size_t)MAX_BONES) continue;
                        int32_t gb=c2g[sw.cluster_index];
                        if(gb<0||gb>=gm->num_gpu_bones) continue;
                        v.bones[out]=gb;
                        v.weights[out]=(float)sw.weight;
                        tw+=(float)sw.weight;
                        out++;
                    }
                    /* Normalize surviving weights; zero-weight verts use identity
                       matrix via the shader's dot(aWeights)<0.05 fallback          */
                    if(tw>0.001f){for(int wi=0;wi<MAX_WEIGHTS;wi++)v.weights[wi]/=tw;}
                    else{for(int wi=0;wi<MAX_WEIGHTS;wi++){v.weights[wi]=0;v.bones[wi]=0;}}
                }
                /* else: no skin data for this vertex → weights stay 0 → identity  */
            }
            if(vc>=vcap){vcap*=2;verts=realloc(verts,vcap*sizeof(Vertex));}
            if(ic>=icap){icap*=2;inds=realloc(inds,icap*sizeof(uint32_t));}
            inds[ic++]=(uint32_t)vc;verts[vc++]=v;
        }
    }
    free(tri);
    if(ic==0||vc==0){
        mesh_normals_free(&mesh_normals);
        free(verts);free(inds);
        return false;
    }
    GLH_Attrib attrs[]={{0,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,pos)},{1,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,normal)},{2,2,GL_FLOAT,GL_FALSE,offsetof(Vertex,uv)},{3,4,GL_FLOAT,GL_FALSE,offsetof(Vertex,weights)},{4,4,GL_INT,0,offsetof(Vertex,bones)},{5,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,tangent)},{6,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,bitangent)}};
    gm->vao=GLH_mesh_full(verts,(size_t)vc*sizeof(Vertex),inds,(size_t)ic*sizeof(uint32_t),sizeof(Vertex),attrs,7,
                          &gm->vbo,&gm->ebo);
    gm->index_count=(GLsizei)ic;gm->geometry_to_world=mat4_from_ufbx(&node->geometry_to_world);gm->node_typed_id=node->typed_id;gm->texture=get_mesh_tex(mesh,node,path,material_index);
    {
        ufbx_material *mat=mesh_material_at(mesh,node,material_index);
        gpu_mesh_apply_pbr(gm,node,mat,path);
    }
    mesh_normals_free(&mesh_normals);
    free(verts);free(inds);return true;
}

static bool scene_load(Scene *s,const char *path){
    char resolved_path[1024];
    asset_resolve_path_copy(resolved_path,sizeof(resolved_path),path);
    const char *load_path=resolved_path[0]?resolved_path:path;
    memset(s,0,sizeof(*s));
    if(path_has_extension_ci(load_path,".modelc"))return scene_load_modelc(s,load_path);
    if(path_has_extension_ci(load_path,".fbbin"))return scene_load_cooked_fbx(s,load_path);
    if(path_has_extension_ci(load_path,".glbbin"))return scene_load_cooked_glb(s,load_path);
    if(path_has_extension_ci(load_path,".gltf")||path_has_extension_ci(load_path,".glb"))return scene_load_gltf(s,load_path);
    ufbx_load_opts opts={0};opts.generate_missing_normals=true;opts.target_axes=ufbx_axes_right_handed_y_up;opts.target_unit_meters=1.0f;opts.use_blender_pbr_material=true;opts.normalize_tangents=true;
    ufbx_error err;s->scene=ufbx_load_file(load_path,&opts,&err);
    if(!s->scene){fprintf(stderr,"Load '%s': %s\n",load_path,err.description.data);return false;}
    s->format=SCENE_FORMAT_UFBX;
    s->num_meshes=0;
    for(size_t ni=0;ni<s->scene->nodes.count&&s->num_meshes<MAX_MESHES;ni++){
        ufbx_node *nd=s->scene->nodes.data[ni];
        if(!nd->mesh)continue;
        size_t mat_count=nd->mesh->materials.count;
        bool split_by_material=mat_count>1&&nd->mesh->face_material.count==nd->mesh->num_faces;
        size_t passes=split_by_material?mat_count:1;
        for(size_t part=0;part<passes&&s->num_meshes<MAX_MESHES;part++){
            int mi=s->num_meshes++;
            if(!build_gpu_mesh(&s->meshes[mi],&s->skins[mi],nd->mesh,nd,load_path,split_by_material?part:0))s->num_meshes--;
        }
    }
    float xmin=1e9f,ymin=1e9f,zmin=1e9f,xmax=-1e9f,ymax=-1e9f,zmax=-1e9f;
    for(size_t ni=0;ni<s->scene->nodes.count;ni++){ufbx_node *nd=s->scene->nodes.data[ni];if(!nd->mesh)continue;
        for(size_t vi=0;vi<nd->mesh->num_vertices;vi++){ufbx_vec3 lp=nd->mesh->vertices.data[vi];ufbx_vec3 p=ufbx_transform_position(&nd->geometry_to_world,lp);
            float x=(float)p.x,y=(float)p.y,z=(float)p.z;if(x<xmin)xmin=x;if(x>xmax)xmax=x;if(y<ymin)ymin=y;if(y>ymax)ymax=y;if(z<zmin)zmin=z;if(z>zmax)zmax=z;}}
    /* Also compute local-space Y bounds (matching build_gpu_mesh vertex storage) for village placement */
    float ymin_local=1e9f,ymax_local=-1e9f;
    for(size_t ni=0;ni<s->scene->nodes.count;ni++){ufbx_node *nd=s->scene->nodes.data[ni];if(!nd->mesh)continue;
        for(size_t fi=0;fi<nd->mesh->num_faces;fi++){ufbx_face face=nd->mesh->faces.data[fi];
            for(size_t i=face.index_begin;i<face.index_begin+face.num_indices;i++){
                float y=(float)ufbx_get_vertex_vec3(&nd->mesh->vertex_position,i).y;
                if(y<ymin_local)ymin_local=y;if(y>ymax_local)ymax_local=y;}}}
    s->ymin_raw=ymin_local; s->ymax_raw=ymax_local;
    printf("[Scene] %s: local_ymin=%.3f local_ymax=%.3f world_ymin=%.3f world_ymax=%.3f\n",
           load_path,(double)ymin_local,(double)ymax_local,(double)ymin,(double)ymax);
    s->cx=(xmin+xmax)*0.5f;s->cy=(ymin+ymax)*0.5f;s->cz=(zmin+zmax)*0.5f;
    s->ymin_raw=ymin; s->ymax_raw=ymax;
    float md=fmaxf(xmax-xmin,fmaxf(ymax-ymin,zmax-zmin));if(md<0.0001f)md=1.0f;
    s->model_scale=2.0f/md;s->ground_offset=-(ymin-s->cy)*s->model_scale;
    printf("[Scene] %s: %d meshes\n",load_path,s->num_meshes);return true;
}
static void scene_free(Scene *s){
    if(s->scene){ufbx_free_scene(s->scene);s->scene=NULL;}
    if(s->gltf){cgltf_free(s->gltf);s->gltf=NULL;}
    free(s->source_blob);s->source_blob=NULL;s->source_blob_size=0;
    free(s->gltf_bind_pose);free(s->gltf_rest_world);free(s->gltf_eval_pose);free(s->gltf_eval_world);free(s->gltf_eval_mark);free(s->gltf_skins);free(s->gltf_tex_cache);
    free(s->modelc_parent_index);free(s->modelc_bind_pose);free(s->modelc_rest_world);free(s->modelc_eval_pose);free(s->modelc_eval_world);free(s->modelc_eval_mark);free(s->modelc_skins);
    if(s->modelc_anims){
        for(int ai=0;ai<s->modelc_anim_count;ai++){
            if(!s->modelc_anims[ai].channels)continue;
            for(uint32_t ci=0;ci<s->modelc_anims[ai].channel_count;ci++){
                free(s->modelc_anims[ai].channels[ci].times);
                free(s->modelc_anims[ai].channels[ci].values);
            }
            free(s->modelc_anims[ai].channels);
        }
    }
    free(s->modelc_anims);
    {
        GLuint seen[MAX_MESHES*4];
        int seen_count=0;
        for(int i=0;i<s->num_meshes;i++){
            glDeleteVertexArrays(1,&s->meshes[i].vao);
            if(s->meshes[i].vbo) glDeleteBuffers(1,&s->meshes[i].vbo);
            if(s->meshes[i].ebo) glDeleteBuffers(1,&s->meshes[i].ebo);
            GLuint tex_ids[4]={s->meshes[i].texture,s->meshes[i].normal_tex,s->meshes[i].mr_tex,s->meshes[i].emissive_tex};
            for(int ti=0;ti<4;ti++){
                GLuint tex=tex_ids[ti];
                if(!tex)continue;
                if(texture_cache_release(tex))continue;
                bool dup=false;
                for(int si=0;si<seen_count;si++)if(seen[si]==tex){dup=true;break;}
                if(dup)continue;
                if(seen_count<(int)(sizeof(seen)/sizeof(seen[0])))seen[seen_count++]=tex;
                glDeleteTextures(1,&tex);
            }
        }
    }
    memset(s,0,sizeof(*s));
}
/* Returns the actual last keyframe time in an animation stack by scanning
   all curves.  Falls back to stk->time_end if no keys are found.
   This fixes the case where the FBX stack duration is longer than the
   actual animation cycle (e.g. walk = 2 s inside a 6 s stack). */
static double anim_stack_real_end(ufbx_anim_stack *stk){
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
    /* If we found no keys at all, fall back to the stack's declared end */
    if(end<=stk->time_begin)end=stk->time_end;
    return end;
}

static void compute_bones(Mat4 *out,int nb,const GpuMesh *gm,ufbx_skin_deformer *sk,ufbx_scene *ev){
    for(int i=0;i<nb;i++)out[i]=mat4_identity();if(!sk||!ev)return;
    uint32_t tid=sk->typed_id;if(tid>=ev->skin_deformers.count)return;
    ufbx_skin_deformer *es=ev->skin_deformers.data[tid];size_t nc=es->clusters.count<(size_t)nb?es->clusters.count:(size_t)nb;
    for(size_t ci=0;ci<nc;ci++){ufbx_skin_cluster *cl=es->clusters.data[ci];int gi=gm->cluster_map[ci];if(gi<0||gi>=nb||!cl->bone_node)continue;
        ufbx_matrix sm=ufbx_matrix_mul(&cl->bone_node->node_to_world,&cl->geometry_to_bone);out[gi]=mat4_from_ufbx(&sm);}
}

/* Immutable geometry and textures are shared. Instance material values remain
   in Prop::meshes, while collision and transforms belong to the instance. */
#define STATIC_MESH_CACHE_CAP 128
#define STATIC_MESH_CACHE_BUDGET (384u*1024u*1024u)
typedef struct{
    bool used;unsigned refs;unsigned long long last_use;
    char path[256];Scene scene;Vec3 min,max;size_t geometry_bytes;
    struct{GLuint id;size_t bytes;}textures[MAX_MESHES*4];int texture_count;
}StaticMeshAsset;
static StaticMeshAsset g_static_mesh_assets[STATIC_MESH_CACHE_CAP];
static unsigned long long g_static_mesh_clock;
static unsigned g_static_mesh_hits,g_static_mesh_misses;

static void static_mesh_measure_gpu(StaticMeshAsset *asset,const Scene *scene){
    asset->geometry_bytes=0;asset->texture_count=0;
    for(int m=0;m<scene->num_meshes;m++){
        const GpuMesh *mesh=&scene->meshes[m];GLint size=0;
        glBindBuffer(GL_ARRAY_BUFFER,mesh->vbo);glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&size);asset->geometry_bytes+=(size_t)size;
        glBindBuffer(GL_ARRAY_BUFFER,mesh->ebo);glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&size);asset->geometry_bytes+=(size_t)size;
        GLuint textures[]={mesh->texture,mesh->normal_tex,mesh->mr_tex,mesh->emissive_tex};
        for(int t=0;t<4;t++){
            if(!textures[t])continue;
            bool duplicate=false;
            for(int i=0;i<asset->texture_count;i++)if(asset->textures[i].id==textures[t])duplicate=true;
            if(duplicate)continue;
            glBindTexture(GL_TEXTURE_2D,textures[t]);GLint w=0,h=0;
            glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&w);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&h);
            asset->textures[asset->texture_count].id=textures[t];
            asset->textures[asset->texture_count++].bytes=(size_t)w*(size_t)h*4u*4u/3u;
        }
    }glBindBuffer(GL_ARRAY_BUFFER,0);
}
static void static_mesh_bounds(StaticMeshAsset *asset){
    asset->min=(Vec3){1e9f,1e9f,1e9f};asset->max=(Vec3){-1e9f,-1e9f,-1e9f};
    const cgltf_data *data=asset->scene.gltf;
    for(size_t n=0;n<data->nodes_count;n++){
        const cgltf_node *node=&data->nodes[n];if(!node->mesh)continue;
        cgltf_float matrix[16];cgltf_node_transform_world(node,matrix);Mat4 world=mat4_from_cgltf(matrix);
        for(size_t p=0;p<node->mesh->primitives_count;p++){
            const cgltf_attribute *attr=gltf_find_attribute(&node->mesh->primitives[p],cgltf_attribute_type_position,0);if(!attr||!attr->data)continue;
            const cgltf_accessor *a=attr->data;
            size_t count=a->has_min&&a->has_max?8:a->count;
            for(size_t v=0;v<count;v++){
                cgltf_float xyz[3];
                if(a->has_min&&a->has_max){for(int c=0;c<3;c++)xyz[c]=(v&(1u<<c))?a->max[c]:a->min[c];}
                else if(!cgltf_accessor_read_float(a,v,xyz,3))continue;
                Vec3 q=mat4_transform_point(world,(Vec3){xyz[0],xyz[1],xyz[2]});
                asset->min.x=fminf(asset->min.x,q.x);asset->min.y=fminf(asset->min.y,q.y);asset->min.z=fminf(asset->min.z,q.z);
                asset->max.x=fmaxf(asset->max.x,q.x);asset->max.y=fmaxf(asset->max.y,q.y);asset->max.z=fmaxf(asset->max.z,q.z);
            }
        }
    }
}
static void static_mesh_cache_metrics(void){
    /* Images may now belong to several different cached GLBs. Count actual
       texture IDs once across owners, without driver queries or allocations. */
    enum{TEXTURE_IDS_CAP=STATIC_MESH_CACHE_CAP*MAX_MESHES*8};
    static GLuint ids[TEXTURE_IDS_CAP];
    static uint32_t marks[TEXTURE_IDS_CAP],stamp;
    if(++stamp==0){memset(marks,0,sizeof(marks));stamp=1;}
    g_engine_perf.static_assets=0;g_engine_perf.static_instances=0;g_engine_perf.static_gpu_bytes=0;
    for(int i=0;i<STATIC_MESH_CACHE_CAP;i++)if(g_static_mesh_assets[i].used){
        StaticMeshAsset *asset=&g_static_mesh_assets[i];
        g_engine_perf.static_assets++;g_engine_perf.static_instances+=asset->refs;
        g_engine_perf.static_gpu_bytes+=asset->geometry_bytes;
        for(int t=0;t<asset->texture_count;t++){
            GLuint id=asset->textures[t].id;
            unsigned slot=(id*2654435761u)&(TEXTURE_IDS_CAP-1u);
            while(marks[slot]==stamp&&ids[slot]!=id)slot=(slot+1u)&(TEXTURE_IDS_CAP-1u);
            if(marks[slot]!=stamp){
                marks[slot]=stamp;ids[slot]=id;
                g_engine_perf.static_gpu_bytes+=asset->textures[t].bytes;
            }
        }
    }
    g_engine_perf.cache_hits=g_static_mesh_hits;g_engine_perf.cache_misses=g_static_mesh_misses;
}
static void static_mesh_cache_evict(int slot){
    StaticMeshAsset *asset=&g_static_mesh_assets[slot];if(!asset->used||asset->refs)return;
    scene_free(&asset->scene);memset(asset,0,sizeof(*asset));
}
static void static_mesh_cache_trim(void){
    static_mesh_cache_metrics();
    while(g_engine_perf.static_gpu_bytes>STATIC_MESH_CACHE_BUDGET){
        int oldest=-1;for(int i=0;i<STATIC_MESH_CACHE_CAP;i++)if(g_static_mesh_assets[i].used&&!g_static_mesh_assets[i].refs&&(oldest<0||g_static_mesh_assets[i].last_use<g_static_mesh_assets[oldest].last_use))oldest=i;
        if(oldest<0)break;
        static_mesh_cache_evict(oldest);static_mesh_cache_metrics();
    }
}
/* On a miss the caller receives an owned animated scene, or a borrowed static
   scene held by the cache. The positive handle distinguishes those lifetimes. */
static bool static_mesh_cache_load(const char *path,Scene *scene,int *handle){
    *handle=0;
    for(int i=0;i<STATIC_MESH_CACHE_CAP;i++){
        StaticMeshAsset *a=&g_static_mesh_assets[i];
        if(a->used&&!strcmp(a->path,path)){a->refs++;a->last_use=++g_static_mesh_clock;*scene=a->scene;*handle=i+1;g_static_mesh_hits++;static_mesh_cache_metrics();return true;}
    }
    if(!scene_load(scene,path))return false;
    if(prop_scene_needs_scene_renderer(scene))return true;
    int slot=-1,oldest=-1;
    for(int i=0;i<STATIC_MESH_CACHE_CAP;i++){
        StaticMeshAsset *a=&g_static_mesh_assets[i];if(!a->used){slot=i;break;}
        if(!a->refs&&(oldest<0||a->last_use<g_static_mesh_assets[oldest].last_use))oldest=i;
    }
    if(slot<0)slot=oldest;
    if(slot<0)return true;
    static_mesh_cache_evict(slot);StaticMeshAsset *a=&g_static_mesh_assets[slot];
    a->used=true;a->refs=1;a->last_use=++g_static_mesh_clock;a->scene=*scene;snprintf(a->path,sizeof(a->path),"%s",path);
    static_mesh_bounds(a);static_mesh_measure_gpu(a,scene);*handle=slot+1;g_static_mesh_misses++;static_mesh_cache_trim();return true;
}
static void static_mesh_cache_release(int handle){
    if(handle<1||handle>STATIC_MESH_CACHE_CAP)return;
    StaticMeshAsset *a=&g_static_mesh_assets[handle-1];
    if(a->used&&a->refs){a->refs--;a->last_use=++g_static_mesh_clock;}static_mesh_cache_trim();
}
static void static_mesh_cache_shutdown(void){
    for(int i=0;i<STATIC_MESH_CACHE_CAP;i++){StaticMeshAsset *a=&g_static_mesh_assets[i];if(a->used){scene_free(&a->scene);memset(a,0,sizeof(*a));}}
    g_static_mesh_hits=g_static_mesh_misses=0;static_mesh_cache_metrics();
}

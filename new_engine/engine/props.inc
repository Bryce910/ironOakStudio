/*
   USAGE in main() - place props before the game loop:

   // Auto-snap Y to terrain, auto AABB:
   prop_place(&g_props,"models/house.fbx",  -1910,0,-1795, 0,  PROP_COLL_MESH, 0,0, 0,0);

   // No collision (fires, barrels, decorations):
   prop_place(&g_props,"models/fire.fbx",   -1900,0,-1800, 45, PROP_COLL_NONE, 0,0, 0,0);

   // Manual AABB (cx,cz=center offset from origin; hw,hd=half width/depth):
   prop_place(&g_props,"models/tower.fbx",  -1890,0,-1810, 0,  PROP_COLL_AABB, 0,0, 3.0,3.0);

   // Per-triangle collision (stairs, arches, complex shapes):
   prop_place(&g_props,"models/steps.fbx",  -1895,0,-1795, 90, PROP_COLL_MESH, 0,0, 0,0);

   COLLISION MODES:
     PROP_COLL_NONE  - no collision (fires, crates, decorations)
     PROP_COLL_AABB  - fast box collision (most buildings)
     PROP_COLL_MESH  - per-triangle (stairs, arches, railings)

   ROTATION: degrees around Y axis
   Y=0 auto-snaps to terrain height
*/
#define MAX_PROPS 1024
typedef enum{PROP_COLL_NONE=0,PROP_COLL_AABB,PROP_COLL_MESH}PropCollType;
typedef enum{PROP_INTERACT_NONE=0,PROP_INTERACT_DOOR}PropInteractType;
typedef enum{
    PROP_ACTION_NONE=0,
    PROP_ACTION_TOGGLE_NEAREST_LIGHT,
    PROP_ACTION_TOGGLE_TARGET_LIGHT,
    PROP_ACTION_TOGGLE_TARGET_COLLISION_VOLUME,
    PROP_ACTION_TOGGLE_TARGET_DOOR,
    PROP_ACTION_SET_WORLD_FLAG,
    PROP_ACTION_CLEAR_WORLD_FLAG,
    PROP_ACTION_PLAY_SOUND,
    PROP_ACTION_READ_PROMPT
}PropActionType;
typedef enum{
    PROP_ACTION_COND_NONE=0,
    PROP_ACTION_COND_FLAG_SET,
    PROP_ACTION_COND_FLAG_CLEAR
}PropActionConditionType;
#define PROP_CUSTOM_COLL_MAX 8
#define PROP_ACTION_MAX 4
#define PROP_ACTION_SOUND_COUNT 10

static const int g_prop_action_sound_ids[PROP_ACTION_SOUND_COUNT]={
    SND_MENU_CLICK,SND_STEP,SND_RUN,SND_HIT,SND_ATTACK,
    SND_JUMP,SND_SHIELD,SND_HEAL,SND_FIREBALL,SND_DASH
};

static const char *g_prop_action_sound_names[PROP_ACTION_SOUND_COUNT]={
    "Menu Click","Step","Run","Hit","Attack",
    "Jump","Shield","Heal","Fireball","Dash"
};

typedef struct{
    int   shape;
    Vec3  center;
    Vec3  half;
    float yaw_deg;
}PropCustomCollisionBox;

typedef enum{
    PROP_CUSTOM_COLL_SHAPE_BOX=0,
    PROP_CUSTOM_COLL_SHAPE_SPHERE,
    PROP_CUSTOM_COLL_SHAPE_CIRCLE,
    PROP_CUSTOM_COLL_SHAPE_OVAL
}PropCustomCollisionShape;

typedef struct{
    PropActionType type;
    float radius;
    int target_index;
    PropActionConditionType condition_type;
    int condition_flag;
    int sound_idx;
    float sound_volume;
    char sound_path[256];
    char prompt_text[512];
}PropAction;

/* ── Streaming states ────────────────────────────────────────── */
typedef enum{
    PROP_UNLOADED=0,  /* definition only, zero GPU memory      */
    PROP_QUEUED,      /* in background parse queue              */
    PROP_READY,       /* CPU data assembled, awaiting GL upload */
    PROP_UPLOADING,   /* main thread is incrementally uploading */
    PROP_LOADED       /* fully on GPU, renders normally         */
}PropStreamState;

/* CPU-side mesh data — produced by background thread, consumed by
   main thread for GL upload.  No GL calls touch this struct.    */
typedef struct{
    Vertex    *verts;
    uint32_t  *inds;
    size_t     vc,ic;
    float      mat_color[4];
    float      alpha_cutoff;
    /* Embedded texture: either raw bytes OR a file path */
    void      *tex_mem;   /* stb_image-decoded RGBA pixels, or NULL */
    int        tex_w,tex_h;
    char       tex_path[512]; /* fallback file path if tex_mem==NULL  */
    Mat4       geometry_to_world;
    uint32_t   node_typed_id;
    int32_t    cluster_map[MAX_BONES];
    int32_t    num_gpu_bones;
}CpuMeshData;

typedef struct{
    int         prop_idx;
    int         prop_uid;
    uint32_t    generation;
    Vec3        pos;
    float       rot_x,rot_y,rot_z;
    float       scale;
    PropCollType coll_type;
    float       aabb_cx,aabb_cz,aabb_hw,aabb_hd;
    char        path[256];
}StreamPropRequest;

static void gltf_texture_to_cpu(const cgltf_texture_view *view,const char *path,CpuMeshData *cm){
    if(!view||!view->texture||!cm)return;
    const cgltf_texture *tex=view->texture;
    const cgltf_image *image=tex->image?tex->image:(tex->has_basisu?tex->basisu_image:(tex->has_webp?tex->webp_image:NULL));
    if(!image)return;
    if(image->buffer_view&&image->buffer_view->buffer&&image->buffer_view->buffer->data){
        const void *data=cgltf_buffer_view_data(image->buffer_view);
        int size=(int)image->buffer_view->size,w=0,h=0,ch=0;
        unsigned char *px=stbi_load_from_memory(data,size,&w,&h,&ch,4);
        if(px){cm->tex_mem=px;cm->tex_w=w;cm->tex_h=h;return;}
    }
    if(image->uri&&image->uri[0]&&strncmp(image->uri,"data:",5)){
        char dir[1024],full[1024];
        model_dir_from_path(dir,sizeof(dir),path);
        snprintf(full,sizeof(full),"%s%s",dir,image->uri);
        path_normalize_slashes(full);
        {
            int w=0,h=0,ch=0;
            unsigned char *px=stbi_load(full,&w,&h,&ch,4);
            if(px){cm->tex_mem=px;cm->tex_w=w;cm->tex_h=h;return;}
        }
        snprintf(cm->tex_path,sizeof(cm->tex_path),"%s",full);
    }
}

static CpuMeshData cpu_mesh_build_gltf(const cgltf_primitive *prim,const cgltf_node *node,const char *path){
    CpuMeshData cm={0};
    const cgltf_attribute *pos_attr=gltf_find_attribute(prim,cgltf_attribute_type_position,0);
    const cgltf_attribute *nrm_attr=gltf_find_attribute(prim,cgltf_attribute_type_normal,0);
    const cgltf_attribute *uv_attr =gltf_find_attribute(prim,cgltf_attribute_type_texcoord,0);
    if(!pos_attr||!pos_attr->data)return cm;
    size_t vcount=pos_attr->data->count;
    size_t icount=prim->indices?prim->indices->count:vcount;
    cm.verts=calloc(vcount,sizeof(Vertex));
    cm.inds=malloc(icount*sizeof(uint32_t));
    if(!cm.verts||!cm.inds){free(cm.verts);free(cm.inds);memset(&cm,0,sizeof(cm));return cm;}
    for(size_t i=0;i<vcount;i++){
        cgltf_float fv[4]={0,0,0,1};
        Vertex *v=&cm.verts[i];
        cgltf_accessor_read_float(pos_attr->data,i,fv,3);v->pos[0]=(float)fv[0];v->pos[1]=(float)fv[1];v->pos[2]=(float)fv[2];
        if(nrm_attr&&cgltf_accessor_read_float(nrm_attr->data,i,fv,3)){v->normal[0]=(float)fv[0];v->normal[1]=(float)fv[1];v->normal[2]=(float)fv[2];}
        else v->normal[1]=1.0f;
        if(uv_attr&&cgltf_accessor_read_float(uv_attr->data,i,fv,2)){v->uv[0]=(float)fv[0];v->uv[1]=(float)fv[1];}
    }
    for(size_t i=0;i<icount;i++)cm.inds[i]=prim->indices?(uint32_t)cgltf_accessor_read_index(prim->indices,i):(uint32_t)i;
    cm.vc=vcount;cm.ic=icount;
    cgltf_float world[16];cgltf_node_transform_world(node,world);
    cm.geometry_to_world=mat4_from_cgltf(world);
    cm.node_typed_id=(uint32_t)(node-node);
    cm.mat_color[0]=cm.mat_color[1]=cm.mat_color[2]=cm.mat_color[3]=1.0f;
    cm.alpha_cutoff=0.0f;
    if(prim->material){
        const cgltf_material *mat=prim->material;
        gltf_texture_to_cpu(&mat->pbr_metallic_roughness.base_color_texture,path,&cm);
        if(mat->has_pbr_metallic_roughness){
            {
                cm.mat_color[0]=(float)mat->pbr_metallic_roughness.base_color_factor[0];
                cm.mat_color[1]=(float)mat->pbr_metallic_roughness.base_color_factor[1];
                cm.mat_color[2]=(float)mat->pbr_metallic_roughness.base_color_factor[2];
            }
            cm.mat_color[3]=(float)mat->pbr_metallic_roughness.base_color_factor[3];
        }
        if(mat->alpha_mode==cgltf_alpha_mode_mask) cm.alpha_cutoff=mat->alpha_cutoff>0.0f?(float)mat->alpha_cutoff:0.5f;
        else if(mat->alpha_mode==cgltf_alpha_mode_blend) cm.alpha_cutoff=0.0f;
    }
    return cm;
}

/* One completed parse job — holds all meshes for one prop */
typedef struct{
    CpuMeshData meshes[MAX_MESHES];
    int         num_meshes;
    int         upload_mesh;
    Vec3        bounds_min;
    Vec3        bounds_max;
    float       ymin;
    float       ymax;
    AABB        aabb;
    TriMesh     trimesh;
    int         prop_idx;   /* index into PropList  */
    int         prop_uid;
    uint32_t    generation;
    bool        valid;
}StreamJob;

typedef struct{
    GpuMesh  meshes[MAX_MESHES];int num_meshes;
    int      uid;
    int      static_asset_handle;
    Scene    scene;
    Vec3     pos;
    float    rot_x,rot_y,rot_z;
    float    scale;
    float    ymin;
    Vec3     bounds_min;
    Vec3     bounds_max;
    PropCollType coll_type;
    /* AABB / trimesh rebuild params */
    float    aabb_cx,aabb_cz,aabb_hw,aabb_hd;
    AABB     aabb;
    TriMesh  trimesh;
    bool     custom_coll_enabled;
    int      custom_coll_count;
    int      custom_coll_selected;
    PropCustomCollisionBox custom_coll[PROP_CUSTOM_COLL_MAX];
    char     path[256];
    PropStreamState stream_state;
    StreamJob *ready_job;  /* non-NULL when PROP_READY, main thread uploads then clears */
    bool     use_scene_renderer;
    bool     pickable;
    ItemType pickup_item;
    int      pickup_count;
    PropInteractType interact_type;
    float    interact_radius;
    float    door_open_angle;
    float    door_closed_yaw;
    bool     door_is_open;
    int      action_count;
    int      action_selected;
    PropAction actions[PROP_ACTION_MAX];
    bool     active;       /* slot in use (even when unloaded) */
}Prop;

typedef struct PropList{Prop list[MAX_PROPS];int count;int selected;}PropList;
#define PROP_VIS_CELL_SIZE 48.0f
#define PROP_VIS_MAX_CELLS MAX_PROPS
typedef struct{
    int gx,gz;
    int head;
    int prop_count;
    AABB bounds;
}PropVisCell;
typedef struct{
    PropList *owner;
    bool dirty;
    int last_count;
    int cell_count;
    int next_prop[MAX_PROPS];
    PropVisCell cells[PROP_VIS_MAX_CELLS];
}PropVisibilityCache;
static GLuint g_prop_prog=0;
static GLint g_prop_loc_model=-1;
static GLint g_prop_loc_albedo=-1;
static GLint g_prop_loc_has_texture=-1;
static GLint g_prop_loc_base_color=-1;
static GLint g_prop_loc_alpha_cutoff=-1;
static GLint g_prop_loc_cam_pos=-1;
static GLint g_prop_loc_normal,g_prop_loc_mr,g_prop_loc_emissive;
static GLint g_prop_loc_has_normal,g_prop_loc_has_mr,g_prop_loc_has_emissive;
static GLint g_prop_loc_metallic,g_prop_loc_roughness,g_prop_loc_emissive_factor;
static GLint g_prop_loc_fog_density,g_prop_loc_fog_color;
static PropList g_props;
static PropVisibilityCache g_prop_vis_cache={0};
extern bool g_render_shadow_pass_wide;
static Mat4 prop_world(Prop *p); /* forward decl — defined below */
static float prop_cull_radius(const Prop *p);
static float prop_cull_center_y(const Prop *p);
static bool prop_place(PropList *pl,const char *fbx_path,
                       float wx,float wy,float wz,
                       float rx,float ry,float rz,float scale,
                       PropCollType coll,
                       float aabb_cx,float aabb_cz,
                       float aabb_hw,float aabb_hd);

static bool prop_uses_scene_renderer(const Prop *p){
    return p && p->use_scene_renderer && scene_has_data((const Scene*)&p->scene);
}

static void props_visibility_invalidate(void){
    g_prop_vis_cache.dirty=true;
}

static bool prop_is_large_visual(const Prop *p){
    float rx,ry,rz;
    if(!p) return false;
    rx=fabsf(p->bounds_max.x-p->bounds_min.x)*fmaxf(p->scale,0.01f);
    ry=fabsf(p->bounds_max.y-p->bounds_min.y)*fmaxf(p->scale,0.01f);
    rz=fabsf(p->bounds_max.z-p->bounds_min.z)*fmaxf(p->scale,0.01f);
    if(p->scale>=2.25f) return true;
    if(rx>=4.0f||ry>=3.5f||rz>=4.0f) return true;
    if(prop_uses_scene_renderer(p)&&p->scale>=1.4f) return true;
    return false;
}

static float prop_render_distance(const Prop *p){
    if(!p) return 0.0f;
    if(prop_is_large_visual(p)) return 520.0f;
    if(p->coll_type!=PROP_COLL_NONE) return 300.0f;
    if(prop_uses_scene_renderer(p)) return 240.0f;
    if(p->scale<0.9f) return 110.0f;
    if(p->scale<1.5f) return 185.0f;
    return 240.0f;
}

static bool prop_should_cast_shadow(const Prop *p,bool wide_pass,float cam_dist2){
    float dist_limit;
    if(!p) return false;
    if(prop_uses_scene_renderer(p)){
        if(!prop_is_large_visual(p)&&p->coll_type==PROP_COLL_NONE) return false;
    }else if(p->coll_type==PROP_COLL_NONE&&!prop_is_large_visual(p)){
        // Small nearby dressing still needs contact shadows for grounding.
        return !wide_pass&&cam_dist2<45.0f*45.0f;
    }
    dist_limit=wide_pass
        ? (prop_is_large_visual(p)?160.0f:0.0f)
        : (prop_is_large_visual(p)?180.0f:(p->coll_type!=PROP_COLL_NONE?110.0f:0.0f));
    if(dist_limit<=0.0f) return false;
    if(cam_dist2>dist_limit*dist_limit) return false;
    return true;
}

static void prop_vis_bounds_reset(AABB *box){
    if(!box) return;
    box->min=(Vec3){1e9f,1e9f,1e9f};
    box->max=(Vec3){-1e9f,-1e9f,-1e9f};
}

static void prop_vis_bounds_expand_sphere(AABB *box,float x,float y,float z,float r){
    if(!box) return;
    if(x-r<box->min.x) box->min.x=x-r;
    if(y-r<box->min.y) box->min.y=y-r;
    if(z-r<box->min.z) box->min.z=z-r;
    if(x+r>box->max.x) box->max.x=x+r;
    if(y+r>box->max.y) box->max.y=y+r;
    if(z+r>box->max.z) box->max.z=z+r;
}

static int prop_vis_find_cell(int gx,int gz){
    for(int i=0;i<g_prop_vis_cache.cell_count;i++){
        if(g_prop_vis_cache.cells[i].gx==gx&&g_prop_vis_cache.cells[i].gz==gz) return i;
    }
    return -1;
}

static int prop_vis_ensure_cell(int gx,int gz){
    int idx=prop_vis_find_cell(gx,gz);
    if(idx>=0) return idx;
    if(g_prop_vis_cache.cell_count>=PROP_VIS_MAX_CELLS) return -1;
    idx=g_prop_vis_cache.cell_count++;
    memset(&g_prop_vis_cache.cells[idx],0,sizeof(g_prop_vis_cache.cells[idx]));
    g_prop_vis_cache.cells[idx].gx=gx;
    g_prop_vis_cache.cells[idx].gz=gz;
    g_prop_vis_cache.cells[idx].head=-1;
    prop_vis_bounds_reset(&g_prop_vis_cache.cells[idx].bounds);
    return idx;
}

static bool prop_vis_cell_valid(const PropVisCell *cell){
    return cell&&cell->prop_count>0&&cell->bounds.min.x<=cell->bounds.max.x;
}

static bool prop_vis_cell_frustum_visible(const float planes[6][4],const PropVisCell *cell){
    Vec3 c;
    float hx,hy,hz,r;
    if(!prop_vis_cell_valid(cell)) return false;
    c.x=(cell->bounds.min.x+cell->bounds.max.x)*0.5f;
    c.y=(cell->bounds.min.y+cell->bounds.max.y)*0.5f;
    c.z=(cell->bounds.min.z+cell->bounds.max.z)*0.5f;
    hx=(cell->bounds.max.x-cell->bounds.min.x)*0.5f;
    hy=(cell->bounds.max.y-cell->bounds.min.y)*0.5f;
    hz=(cell->bounds.max.z-cell->bounds.min.z)*0.5f;
    r=sqrtf(hx*hx+hy*hy+hz*hz);
    return frustum_sphere((float (*)[4])planes,c.x,c.y,c.z,fmaxf(r,4.0f));
}

static float prop_vis_cell_dist2_xz(const PropVisCell *cell,Vec3 cam){
    float cx,cz,dx,dz;
    if(!prop_vis_cell_valid(cell)) return 0.0f;
    cx=clampf(cam.x,cell->bounds.min.x,cell->bounds.max.x);
    cz=clampf(cam.z,cell->bounds.min.z,cell->bounds.max.z);
    dx=cx-cam.x;
    dz=cz-cam.z;
    return dx*dx+dz*dz;
}

static void props_visibility_rebuild(PropList *pl){
    if(!pl) return;
    if(!g_prop_vis_cache.dirty&&g_prop_vis_cache.owner==pl&&g_prop_vis_cache.last_count==pl->count) return;
    g_prop_vis_cache.owner=pl;
    g_prop_vis_cache.last_count=pl->count;
    g_prop_vis_cache.cell_count=0;
    for(int i=0;i<MAX_PROPS;i++) g_prop_vis_cache.next_prop[i]=-1;
    for(int i=0;i<pl->count;i++){
        Prop *p=&pl->list[i];
        float cull_r,cull_y;
        int gx,gz,cell_idx;
        PropVisCell *cell;
        if(!p->active) continue;
        gx=(int)floorf(p->pos.x/PROP_VIS_CELL_SIZE);
        gz=(int)floorf(p->pos.z/PROP_VIS_CELL_SIZE);
        cell_idx=prop_vis_ensure_cell(gx,gz);
        if(cell_idx<0) continue;
        cell=&g_prop_vis_cache.cells[cell_idx];
        cull_r=prop_cull_radius(p);
        cull_y=prop_cull_center_y(p);
        prop_vis_bounds_expand_sphere(&cell->bounds,p->pos.x,cull_y,p->pos.z,cull_r);
        g_prop_vis_cache.next_prop[i]=cell->head;
        cell->head=i;
        cell->prop_count++;
    }
    g_prop_vis_cache.dirty=false;
}

static float prop_cull_radius(const Prop *p){
    float ex,ey,ez;
    if(!p) return 30.0f;
    ex=(p->bounds_max.x-p->bounds_min.x)*0.5f;
    ey=(p->bounds_max.y-p->bounds_min.y)*0.5f;
    ez=(p->bounds_max.z-p->bounds_min.z)*0.5f;
    return fmaxf(6.0f,sqrtf(ex*ex+ey*ey+ez*ez)*fmaxf(p->scale,0.01f)+2.0f);
}

static float prop_cull_center_y(const Prop *p){
    float cy_local;
    if(!p) return 0.0f;
    cy_local=(p->bounds_min.y+p->bounds_max.y)*0.5f-p->ymin;
    return p->pos.y+cy_local*fmaxf(p->scale,0.01f);
}

static bool prop_path_is_likely_skinned(const char *path){
    char resolved[1024]={0};
    FILE *f=NULL;
    unsigned char *buf=NULL;
    long sz=0;
    size_t rd=0;
    bool skinned=false;
    if(!path||!path[0]) return false;
    if(!(path_has_extension_ci(path,".gltf")||path_has_extension_ci(path,".glb"))) return false;
    asset_resolve_path_copy(resolved,sizeof(resolved),path);
    f=fopen(resolved[0]?resolved:path,"rb");
    if(!f) return false;
    if(fseek(f,0,SEEK_END)!=0){ fclose(f); return false; }
    sz=ftell(f);
    if(sz<0){ fclose(f); return false; }
    if(fseek(f,0,SEEK_SET)!=0){ fclose(f); return false; }
    if(sz>1024*1024) sz=1024*1024;
    buf=(unsigned char*)malloc((size_t)sz+1u);
    if(!buf){ fclose(f); return false; }
    rd=fread(buf,1,(size_t)sz,f);
    fclose(f);
    buf[rd]=0;
    if(rd>0){
        const char *s=(const char*)buf;
        skinned=(strstr(s,"JOINTS_0")!=NULL)||
                (strstr(s,"WEIGHTS_0")!=NULL)||
                (strstr(s,"\"skins\"")!=NULL)||
                (strstr(s,"\"skin\":")!=NULL);
    }
    free(buf);
    return skinned;
}

static bool prop_scene_needs_scene_renderer(const Scene *sc){
    int mi=0;
    if(!scene_has_data(sc)) return false;
    if(sc->gltf_skin_count>0 || scene_anim_count(sc)>0) return true;
    for(mi=0;mi<sc->num_meshes;mi++) if(sc->meshes[mi].num_gpu_bones>0) return true;
    return false;
}

#include "engine/static_mesh_cache.inc"

static void prop_release_all(Prop *p);
static bool prop_has_custom_collision(const Prop *p);
static void prop_custom_collision_world_aabb(const Prop *p,const PropCustomCollisionBox *box,AABB *out_box);
static void prop_init_interaction_defaults(Prop *p);
static int prop_custom_collision_shape_sanitize(int shape);
static const char *prop_custom_collision_shape_name(int shape);

static int prop_custom_collision_shape_sanitize(int shape){
    switch(shape){
        case PROP_CUSTOM_COLL_SHAPE_BOX:
        case PROP_CUSTOM_COLL_SHAPE_SPHERE:
        case PROP_CUSTOM_COLL_SHAPE_CIRCLE:
        case PROP_CUSTOM_COLL_SHAPE_OVAL:
            return shape;
        default:
            return PROP_CUSTOM_COLL_SHAPE_BOX;
    }
}

static const char *prop_custom_collision_shape_name(int shape){
    switch(prop_custom_collision_shape_sanitize(shape)){
        case PROP_CUSTOM_COLL_SHAPE_SPHERE: return "Sphere";
        case PROP_CUSTOM_COLL_SHAPE_CIRCLE: return "Circle";
        case PROP_CUSTOM_COLL_SHAPE_OVAL: return "Oval";
        default: return "Box";
    }
}

static bool prop_has_custom_collision(const Prop *p){
    return p&&p->custom_coll_enabled&&p->custom_coll_count>0;
}

static void prop_init_interaction_defaults(Prop *p){
    if(!p) return;
    p->interact_type=PROP_INTERACT_NONE;
    p->interact_radius=2.5f;
    p->door_open_angle=90.0f;
    p->door_closed_yaw=p->rot_y;
    p->door_is_open=false;
    p->action_count=0;
    p->action_selected=0;
    memset(p->actions,0,sizeof(p->actions));
}

static void prop_custom_collision_default_box(const Prop *p,PropCustomCollisionBox *box){
    Vec3 min_v,max_v,center,half;
    if(!p||!box) return;
    min_v=p->bounds_min;
    max_v=p->bounds_max;
    center=(Vec3){
        0.5f*(min_v.x+max_v.x),
        0.5f*(min_v.y+max_v.y),
        0.5f*(min_v.z+max_v.z)
    };
    half=(Vec3){
        fmaxf(0.25f,0.5f*(max_v.x-min_v.x)),
        fmaxf(0.25f,0.5f*(max_v.y-min_v.y)),
        fmaxf(0.25f,0.5f*(max_v.z-min_v.z))
    };
    box->center=center;
    box->half=half;
    box->shape=PROP_CUSTOM_COLL_SHAPE_BOX;
    box->yaw_deg=0.0f;
}

static void prop_custom_collision_clamp_box(PropCustomCollisionBox *box){
    float radius;
    if(!box) return;
    box->shape=prop_custom_collision_shape_sanitize(box->shape);
    box->half.x=fmaxf(0.05f,box->half.x);
    box->half.y=fmaxf(0.05f,box->half.y);
    box->half.z=fmaxf(0.05f,box->half.z);
    if(box->shape==PROP_CUSTOM_COLL_SHAPE_SPHERE){
        radius=fmaxf(box->half.x,fmaxf(box->half.y,box->half.z));
        box->half.x=radius;
        box->half.y=radius;
        box->half.z=radius;
        box->yaw_deg=0.0f;
    }else if(box->shape==PROP_CUSTOM_COLL_SHAPE_CIRCLE){
        radius=fmaxf(box->half.x,box->half.z);
        box->half.x=radius;
        box->half.z=radius;
        box->yaw_deg=0.0f;
    }
    while(box->yaw_deg>180.0f) box->yaw_deg-=360.0f;
    while(box->yaw_deg<-180.0f) box->yaw_deg+=360.0f;
}

static void prop_custom_collision_world_aabb(const Prop *p,const PropCustomCollisionBox *box,AABB *out_box){
    Vec3 corners[8];
    Mat4 local,world;
    float mnx=1e9f,mny=1e9f,mnz=1e9f,mxx=-1e9f,mxy=-1e9f,mxz=-1e9f;
    if(!p||!box||!out_box) return;
    if(box->shape==PROP_CUSTOM_COLL_SHAPE_SPHERE||box->shape==PROP_CUSTOM_COLL_SHAPE_CIRCLE){
        Vec3 world_center=mat4_transform_point(prop_world((Prop*)p),box->center);
        float scaled_x=fabsf(p->scale)*box->half.x;
        float scaled_y=fabsf(p->scale)*box->half.y;
        float scaled_z=fabsf(p->scale)*box->half.z;
        out_box->min=(Vec3){world_center.x-scaled_x,world_center.y-scaled_y,world_center.z-scaled_z};
        out_box->max=(Vec3){world_center.x+scaled_x,world_center.y+scaled_y,world_center.z+scaled_z};
        return;
    }
    corners[0]=(Vec3){-box->half.x,-box->half.y,-box->half.z};
    corners[1]=(Vec3){ box->half.x,-box->half.y,-box->half.z};
    corners[2]=(Vec3){-box->half.x, box->half.y,-box->half.z};
    corners[3]=(Vec3){ box->half.x, box->half.y,-box->half.z};
    corners[4]=(Vec3){-box->half.x,-box->half.y, box->half.z};
    corners[5]=(Vec3){ box->half.x,-box->half.y, box->half.z};
    corners[6]=(Vec3){-box->half.x, box->half.y, box->half.z};
    corners[7]=(Vec3){ box->half.x, box->half.y, box->half.z};
    local=mat4_mul(mat4_translate(box->center.x,box->center.y,box->center.z),
                   mat4_rot_y(box->yaw_deg*(float)M_PI/180.0f));
    world=mat4_mul(prop_world((Prop*)p),local);
    for(int i=0;i<8;i++){
        Vec3 wp=mat4_transform_point(world,corners[i]);
        if(wp.x<mnx) mnx=wp.x; if(wp.x>mxx) mxx=wp.x;
        if(wp.y<mny) mny=wp.y; if(wp.y>mxy) mxy=wp.y;
        if(wp.z<mnz) mnz=wp.z; if(wp.z>mxz) mxz=wp.z;
    }
    out_box->min=(Vec3){mnx,mny,mnz};
    out_box->max=(Vec3){mxx,mxy,mxz};
}

static bool prop_name_suggests_alpha_cutout(const char *text){
    const char *s=text?text:"";
    return path_contains_ci(s,"leaf")||
           path_contains_ci(s,"foliage")||
           path_contains_ci(s,"grass")||
           path_contains_ci(s,"flower")||
           path_contains_ci(s,"bush")||
           path_contains_ci(s,"hedge")||
           path_contains_ci(s,"plant")||
           path_contains_ci(s,"vine")||
           path_contains_ci(s,"ivy")||
           path_contains_ci(s,"branch")||
           path_contains_ci(s,"frond")||
           path_contains_ci(s,"fern");
}

static bool prop_mesh_is_two_sided(const GpuMesh *gm){
    return gm && (gm->double_sided || gm->alpha_cutoff>0.001f || gm->mat_color[3]<0.999f);
}

/* ── Background streaming thread ────────────────────────────────
   One worker thread.  Main thread pushes prop indices to load;
   worker parses FBX + assembles CPU data; main thread picks up
   completed jobs and does the GL upload (max STREAM_GPU_PER_FRAME
   per frame to avoid stutter).                                   */
#define STREAM_LOAD_DIST     200.0f  /* start loading when within 200 units  */
#define STREAM_UNLOAD_DIST   280.0f  /* free GPU when beyond 280 units       */
#define STREAM_GPU_PER_FRAME 1       /* cap uploads harder to reduce main-thread hitches */
#define STREAM_QUEUE_CAP     64      /* room for more simultaneous requests  */

int   g_props_stream_last_uploads=0;
float g_props_stream_last_upload_ms=0.0f;

typedef struct{
    /* Request queue: main → worker */
    StreamPropRequest req[STREAM_QUEUE_CAP];
    int       req_head,req_tail,req_count;
    /* Ready queue: worker → main */
    StreamJob *ready[STREAM_QUEUE_CAP];
    int       rdy_head,rdy_tail,rdy_count;
    SDL_mutex *mutex;
    SDL_sem   *work_sem;   /* signalled when request added   */
    SDL_Thread *thread;
    bool       quit;
    uint32_t   generation;
    PropList  *props;      /* shared read-only reference     */
}StreamSystem;

static StreamSystem g_stream;

static void stream_job_free(StreamJob *job){
    if(!job)return;
    for(int i=0;i<job->num_meshes;i++){
        free(job->meshes[i].verts);
        free(job->meshes[i].inds);
        if(job->meshes[i].tex_mem) stbi_image_free(job->meshes[i].tex_mem);
    }
    trimesh_free(&job->trimesh);
    free(job);
}

static Mat4 stream_prop_world_req(const StreamPropRequest *p,float ymin){
    float d2r=(float)M_PI/180.0f;
    Mat4 lift =mat4_translate(0,-ymin,0);
    Mat4 sc   =mat4_scale_xyz(p->scale,p->scale,p->scale);
    Mat4 rx   =mat4_rot_x(p->rot_x*d2r);
    Mat4 ry   =mat4_rot_y(p->rot_y*d2r);
    Mat4 rz   =mat4_rot_z(p->rot_z*d2r);
    Mat4 rot  =mat4_mul(ry,mat4_mul(rx,rz));
    Mat4 trans=mat4_translate(p->pos.x,p->pos.y,p->pos.z);
    return mat4_mul(trans,mat4_mul(sc,mat4_mul(rot,lift)));
}

static void prop_compute_world_bounds_box(const Prop *p,AABB *out_box){
    float wxmn=1e9f,wymn=1e9f,wzmn=1e9f,wxmx=-1e9f,wymx=-1e9f,wzmx=-1e9f;
    Vec3 corners[8];
    Mat4 world;
    if(!p||!out_box) return;
    corners[0]=(Vec3){p->bounds_min.x,p->bounds_min.y,p->bounds_min.z};
    corners[1]=(Vec3){p->bounds_max.x,p->bounds_min.y,p->bounds_min.z};
    corners[2]=(Vec3){p->bounds_min.x,p->bounds_max.y,p->bounds_min.z};
    corners[3]=(Vec3){p->bounds_max.x,p->bounds_max.y,p->bounds_min.z};
    corners[4]=(Vec3){p->bounds_min.x,p->bounds_min.y,p->bounds_max.z};
    corners[5]=(Vec3){p->bounds_max.x,p->bounds_min.y,p->bounds_max.z};
    corners[6]=(Vec3){p->bounds_min.x,p->bounds_max.y,p->bounds_max.z};
    corners[7]=(Vec3){p->bounds_max.x,p->bounds_max.y,p->bounds_max.z};
    world=prop_world((Prop*)p);
    for(int i=0;i<8;i++){
        Vec3 tp=mat4_transform_point(world,corners[i]);
        if(tp.x<wxmn)wxmn=tp.x;if(tp.x>wxmx)wxmx=tp.x;
        if(tp.y<wymn)wymn=tp.y;if(tp.y>wymx)wymx=tp.y;
        if(tp.z<wzmn)wzmn=tp.z;if(tp.z>wzmx)wzmx=tp.z;
    }
    out_box->min=(Vec3){wxmn,wymn,wzmn};
    out_box->max=(Vec3){wxmx,wymx,wzmx};
}

static void prop_rebuild_aabb_from_bounds(Prop *p){
    if(!p||p->coll_type!=PROP_COLL_AABB) return;
    if(p->aabb_hw>0.0f){
        float h=(p->bounds_max.y-p->bounds_min.y)*p->scale;
        p->aabb.min=(Vec3){p->pos.x+p->aabb_cx-p->aabb_hw,p->pos.y,p->pos.z+p->aabb_cz-p->aabb_hd};
        p->aabb.max=(Vec3){p->pos.x+p->aabb_cx+p->aabb_hw,p->pos.y+h,p->pos.z+p->aabb_cz+p->aabb_hd};
        return;
    }
    prop_compute_world_bounds_box(p,&p->aabb);
}

static void prop_refresh_collision(Prop *p){
    if(!p||!p->active) return;
    if(p->coll_type==PROP_COLL_AABB){
        prop_rebuild_aabb_from_bounds(p);
    }else if(p->coll_type==PROP_COLL_MESH&&p->use_scene_renderer){
        trimesh_free(&p->trimesh);
        if(p->scene.scene) trimesh_build(&p->trimesh,p->scene.scene);
        else if(p->scene.gltf) trimesh_build_gltf(&p->trimesh,p->scene.gltf);
        trimesh_apply_transform(&p->trimesh,prop_world(p));
    }
    static_collision_world_mark_dirty();
}

static void stream_job_compute_aabb(const StreamPropRequest *p,StreamJob *job){
    if(!p||!job||p->coll_type!=PROP_COLL_AABB) return;
    if(p->aabb_hw>0.0f){
        float h=(job->ymax-job->ymin)*p->scale;
        job->aabb.min=(Vec3){p->pos.x+p->aabb_cx-p->aabb_hw,p->pos.y,p->pos.z+p->aabb_cz-p->aabb_hd};
        job->aabb.max=(Vec3){p->pos.x+p->aabb_cx+p->aabb_hw,p->pos.y+h,p->pos.z+p->aabb_cz+p->aabb_hd};
        return;
    }
    {
        float wxmn=1e9f,wymn=1e9f,wzmn=1e9f,wxmx=-1e9f,wymx=-1e9f,wzmx=-1e9f;
        Mat4 world=stream_prop_world_req(p,job->ymin);
        for(int mi=0;mi<job->num_meshes;mi++){
            CpuMeshData *cm=&job->meshes[mi];
            Mat4 M=mat4_mul(world,cm->geometry_to_world);
            for(size_t vi=0;vi<cm->vc;vi++){
                float vx=cm->verts[vi].pos[0];
                float vy=cm->verts[vi].pos[1];
                float vz=cm->verts[vi].pos[2];
                float tx=M.m[0]*vx+M.m[4]*vy+M.m[8]*vz+M.m[12];
                float ty=M.m[1]*vx+M.m[5]*vy+M.m[9]*vz+M.m[13];
                float tz=M.m[2]*vx+M.m[6]*vy+M.m[10]*vz+M.m[14];
                if(tx<wxmn)wxmn=tx;if(tx>wxmx)wxmx=tx;
                if(ty<wymn)wymn=ty;if(ty>wymx)wymx=ty;
                if(tz<wzmn)wzmn=tz;if(tz>wzmx)wzmx=tz;
            }
        }
        job->aabb.min=(Vec3){wxmn,wymn,wzmn};
        job->aabb.max=(Vec3){wxmx,wymx,wzmx};
    }
}

/* Background thread: parse FBX on CPU, no GL calls */
static CpuMeshData cpu_mesh_build(ufbx_mesh *mesh,ufbx_node *node,const char *path){
    CpuMeshData cm={0};
    ufbx_skin_deformer *skin=mesh->skin_deformers.count>0?mesh->skin_deformers.data[0]:NULL;
    MeshNormals mesh_normals=mesh_normals_build(mesh);
    const ufbx_vertex_vec2 *uv_stream=mesh_preferred_uv(mesh);
    int32_t c2g[MAX_BONES];memset(c2g,-1,sizeof(c2g));
    cm.num_gpu_bones=0;
    if(skin){
        size_t nc=skin->clusters.count<MAX_BONES?skin->clusters.count:MAX_BONES;
        for(size_t ci=0;ci<nc;ci++){c2g[ci]=(int32_t)ci;cm.cluster_map[ci]=(int32_t)ci;}
        cm.num_gpu_bones=(int32_t)nc;
    }
    size_t ntri=mesh->max_face_triangles*3;
    uint32_t *tri=malloc(ntri*sizeof(uint32_t));if(!tri){mesh_normals_free(&mesh_normals);return cm;}
    size_t vcap=4096,icap=4096;
    cm.verts=malloc(vcap*sizeof(Vertex));
    cm.inds =malloc(icap*sizeof(uint32_t));
    if(!cm.verts||!cm.inds){free(tri);free(cm.verts);free(cm.inds);mesh_normals_free(&mesh_normals);memset(&cm,0,sizeof(cm));return cm;}
    for(size_t fi=0;fi<mesh->num_faces;fi++){
        ufbx_face face=mesh->faces.data[fi];
        size_t nt=ufbx_triangulate_face(tri,ntri,mesh,face);
        for(size_t ti=0;ti<nt*3;ti++){
            uint32_t ix=tri[ti];uint32_t vi=mesh->vertex_indices.data[ix];Vertex v={0};
            ufbx_vec3 p=ufbx_get_vertex_vec3(&mesh->vertex_position,ix);
            v.pos[0]=(float)p.x;v.pos[1]=(float)p.y;v.pos[2]=(float)p.z;
            ufbx_vec3 n=mesh_normal_get(&mesh_normals,mesh,ix);
            v.normal[0]=(float)n.x;v.normal[1]=(float)n.y;v.normal[2]=(float)n.z;
            if(uv_stream&&uv_stream->exists){ufbx_vec2 uv=ufbx_get_vertex_vec2(uv_stream,ix);v.uv[0]=(float)uv.x;v.uv[1]=(float)uv.y;}
            if(skin&&vi<skin->vertices.count){
                ufbx_skin_vertex sv=skin->vertices.data[vi];
                size_t nw=sv.num_weights<MAX_WEIGHTS?sv.num_weights:MAX_WEIGHTS;
                float tw=0;int out=0;
                for(size_t wi=0;wi<nw&&out<MAX_WEIGHTS;wi++){
                    ufbx_skin_weight sw=skin->weights.data[sv.weight_begin+wi];
                    if(sw.cluster_index>=(size_t)MAX_BONES)continue;
                    int32_t gb=c2g[sw.cluster_index];if(gb<0||gb>=cm.num_gpu_bones)continue;
                    v.bones[out]=gb;v.weights[out]=(float)sw.weight;tw+=(float)sw.weight;out++;
                }
                if(tw>0.001f){for(int wi=0;wi<MAX_WEIGHTS;wi++)v.weights[wi]/=tw;}
                else{for(int wi=0;wi<MAX_WEIGHTS;wi++){v.weights[wi]=0;v.bones[wi]=0;}}
            }
            if(cm.vc>=vcap){vcap*=2;cm.verts=realloc(cm.verts,vcap*sizeof(Vertex));}
            if(cm.ic>=icap){icap*=2;cm.inds=realloc(cm.inds,icap*sizeof(uint32_t));}
            cm.inds[cm.ic++]=(uint32_t)cm.vc;cm.verts[cm.vc++]=v;
        }
    }
    free(tri);
    cm.geometry_to_world=mat4_from_ufbx(&node->geometry_to_world);
    cm.node_typed_id=node->typed_id;
    /* Material color */
    cm.mat_color[0]=0.75f;cm.mat_color[1]=0.72f;cm.mat_color[2]=0.68f;cm.mat_color[3]=1.0f;
    cm.alpha_cutoff=0.0f;
    for(size_t mi=0;mi<mesh->materials.count;mi++){
        ufbx_material *mat=mesh->materials.data[mi];if(!mat)continue;
        /* Try to extract embedded texture pixels (stb_image — no GL) */
        ufbx_texture *tex=mat->pbr.base_color.texture;if(!tex)tex=mat->fbx.diffuse_color.texture;
            if(tex){
                if(tex->content.size>4){
                    int w,h,ch;
                    unsigned char *px=stbi_load_from_memory(tex->content.data,(int)tex->content.size,&w,&h,&ch,4);
                    if(px){cm.tex_mem=px;cm.tex_w=w;cm.tex_h=h;}
                }
                if(!cm.tex_mem&&tex->relative_filename.length>0){
                    texture_path_resolve(cm.tex_path,sizeof(cm.tex_path),path,tex->relative_filename);
                    if(cm.tex_path[0]){
                        int w=0,h=0,ch=0;
                        unsigned char *px=stbi_load(cm.tex_path,&w,&h,&ch,4);
                        if(px){cm.tex_mem=px;cm.tex_w=w;cm.tex_h=h;cm.tex_path[0]='\0';}
                    }
                }
            }
        /* RGB base_color ignored — source assets frequently have wrong tints.
           mat_color stays at the neutral default (0.75,0.72,0.68); texture owns color. */
        if(mat->pbr.opacity.has_value) cm.mat_color[3]=clampf((float)mat->pbr.opacity.value_real,0.0f,1.0f);
        if(prop_name_suggests_alpha_cutout(node&&node->name.data?node->name.data:"")||
           prop_name_suggests_alpha_cutout(mat->name.data?mat->name.data:"")||
           prop_name_suggests_alpha_cutout(cm.tex_path)){
            cm.alpha_cutoff=0.33f;
        }
        break;
    }
    mesh_normals_free(&mesh_normals);
    return cm;
}

static int stream_worker_thread(void *arg){
    StreamSystem *ss=(StreamSystem*)arg;
    while(1){
        SDL_SemWait(ss->work_sem);
        if(ss->quit)break;

        /* Dequeue one request */
        StreamPropRequest req;
        SDL_LockMutex(ss->mutex);
        if(ss->req_count==0){SDL_UnlockMutex(ss->mutex);continue;}
        req=ss->req[ss->req_head%STREAM_QUEUE_CAP];
        ss->req_head++;ss->req_count--;
        SDL_UnlockMutex(ss->mutex);

        StreamJob *job=calloc(1,sizeof(StreamJob));
        if(!job)continue;
        job->prop_idx=req.prop_idx;
        job->prop_uid=req.prop_uid;
        job->generation=req.generation;

        if(path_has_extension_ci(req.path,".gltf")||path_has_extension_ci(req.path,".glb")){
            cgltf_options gopts={0};
            cgltf_data *gltf=NULL;
            cgltf_result gres=cgltf_parse_file(&gopts,req.path,&gltf);
            if(gres!=cgltf_result_success||!gltf||cgltf_load_buffers(&gopts,gltf,req.path)!=cgltf_result_success){
                fprintf(stderr,"[Stream] Load '%s': cgltf failed\n",req.path);
                SDL_LockMutex(ss->mutex);
                if(ss->generation==req.generation&&ss->props&&req.prop_idx>=0&&req.prop_idx<ss->props->count){
                    Prop *p=&ss->props->list[req.prop_idx];
                    if(p->active&&p->uid==req.prop_uid&&p->stream_state==PROP_QUEUED) p->stream_state=PROP_UNLOADED;
                }
                SDL_UnlockMutex(ss->mutex);
                if(gltf)cgltf_free(gltf);
                free(job);continue;
            }
            float xmn=1e9f,ymn=1e9f,zmn=1e9f,xmx=-1e9f,ymx=-1e9f,zmx=-1e9f;
            for(size_t ni=0;ni<gltf->nodes_count;ni++){
                cgltf_node *nd=&gltf->nodes[ni];if(!nd->mesh)continue;
                cgltf_float world[16];cgltf_node_transform_world(nd,world);Mat4 wm=mat4_from_cgltf(world);
                for(size_t pi=0;pi<nd->mesh->primitives_count;pi++){
                    const cgltf_attribute *pos_attr=gltf_find_attribute(&nd->mesh->primitives[pi],cgltf_attribute_type_position,0);
                    if(!pos_attr||!pos_attr->data)continue;
                    for(size_t vi=0;vi<pos_attr->data->count;vi++){
                        cgltf_float pv[3]={0};cgltf_accessor_read_float(pos_attr->data,vi,pv,3);
                        float x=wm.m[0]*pv[0]+wm.m[4]*pv[1]+wm.m[8]*pv[2]+wm.m[12];
                        float y=wm.m[1]*pv[0]+wm.m[5]*pv[1]+wm.m[9]*pv[2]+wm.m[13];
                        float z=wm.m[2]*pv[0]+wm.m[6]*pv[1]+wm.m[10]*pv[2]+wm.m[14];
                        if(x<xmn)xmn=x;
                        if(y<ymn)ymn=y;
                        if(z<zmn)zmn=z;
                        if(x>xmx)xmx=x;
                        if(y>ymx)ymx=y;
                        if(z>zmx)zmx=z;
                    }
                    if(job->num_meshes<MAX_MESHES){
                        job->meshes[job->num_meshes]=cpu_mesh_build_gltf(&nd->mesh->primitives[pi],nd,req.path);
                        if(job->meshes[job->num_meshes].verts)job->num_meshes++;
                    }
                }
            }
            job->bounds_min=(Vec3){xmn,ymn,zmn};
            job->bounds_max=(Vec3){xmx,ymx,zmx};
            job->ymin=ymn;
            job->ymax=ymx;
            if(req.coll_type==PROP_COLL_MESH){
                trimesh_build_gltf(&job->trimesh,gltf);
                float d2r=(float)3.14159265f/180.0f;
                float lift=-job->ymin;
                Mat4 liftM=mat4_translate(0,lift,0);
                Mat4 sc2=mat4_scale_xyz(req.scale,req.scale,req.scale);
                Mat4 rot=mat4_mul(mat4_rot_y(req.rot_y*d2r),mat4_mul(mat4_rot_x(req.rot_x*d2r),mat4_rot_z(req.rot_z*d2r)));
                Mat4 trans=mat4_translate(req.pos.x,req.pos.y,req.pos.z);
                Mat4 wmat=mat4_mul(trans,mat4_mul(sc2,mat4_mul(rot,liftM)));
                trimesh_apply_transform(&job->trimesh,wmat);
            }
            cgltf_free(gltf);
        }else{
            ufbx_load_opts opts={0};
            opts.generate_missing_normals=true;
            opts.target_axes=ufbx_axes_right_handed_y_up;
            opts.target_unit_meters=1.0f;
            ufbx_error err;
            ufbx_scene *sc=ufbx_load_file(req.path,&opts,&err);
            if(!sc){
                fprintf(stderr,"[Stream] Load '%s': %s\n",req.path,err.description.data);
                /* Mark back to unloaded so it can be retried */
                SDL_LockMutex(ss->mutex);
                if(ss->generation==req.generation&&ss->props&&req.prop_idx>=0&&req.prop_idx<ss->props->count){
                    Prop *p=&ss->props->list[req.prop_idx];
                    if(p->active&&p->uid==req.prop_uid&&p->stream_state==PROP_QUEUED) p->stream_state=PROP_UNLOADED;
                }
                SDL_UnlockMutex(ss->mutex);
                free(job);continue;
            }

            /* Compute Y bounds for ground-snap — transform through geometry_to_world
               so axis conversion is applied (raw .y is Z in Blender Z-up space)  */
            float xmn=1e9f,ymn=1e9f,zmn=1e9f,xmx=-1e9f,ymx=-1e9f,zmx=-1e9f;
            for(size_t ni=0;ni<sc->nodes.count;ni++){
                ufbx_node *nd=sc->nodes.data[ni];if(!nd->mesh)continue;
                for(size_t fi=0;fi<nd->mesh->num_faces;fi++){
                    ufbx_face face=nd->mesh->faces.data[fi];
                    for(size_t i=face.index_begin;i<face.index_begin+face.num_indices;i++){
                        ufbx_vec3 lp=ufbx_get_vertex_vec3(&nd->mesh->vertex_position,i);
                        ufbx_vec3 wp=ufbx_transform_position(&nd->geometry_to_world,lp);
                        float x=(float)wp.x;
                        float y=(float)wp.y;
                        float z=(float)wp.z;
                        if(x<xmn)xmn=x;
                        if(y<ymn)ymn=y;if(y>ymx)ymx=y;
                        if(z<zmn)zmn=z;
                        if(x>xmx)xmx=x;
                        if(z>zmx)zmx=z;
                    }
                }
            }
            job->bounds_min=(Vec3){xmn,ymn,zmn};
            job->bounds_max=(Vec3){xmx,ymx,zmx};
            job->ymin=ymn;
            job->ymax=ymx;

            /* Build CPU meshes */
            for(size_t ni=0;ni<sc->nodes.count&&job->num_meshes<MAX_MESHES;ni++){
                ufbx_node *nd=sc->nodes.data[ni];if(!nd->mesh)continue;
                job->meshes[job->num_meshes]=cpu_mesh_build(nd->mesh,nd,req.path);
                if(job->meshes[job->num_meshes].verts)job->num_meshes++;
            }

            if(req.coll_type==PROP_COLL_MESH){
                trimesh_build(&job->trimesh,sc);
                /* Build world matrix matching prop_world(p) with job->ymin */
                {
                    float d2r=(float)3.14159265f/180.0f;
                    float lift=-job->ymin;
                    Mat4 liftM=mat4_translate(0,lift,0);
                    Mat4 sc2=mat4_scale_xyz(req.scale,req.scale,req.scale);
                    Mat4 rot=mat4_mul(mat4_rot_y(req.rot_y*d2r),mat4_mul(mat4_rot_x(req.rot_x*d2r),mat4_rot_z(req.rot_z*d2r)));
                    Mat4 trans=mat4_translate(req.pos.x,req.pos.y,req.pos.z);
                    Mat4 wmat=mat4_mul(trans,mat4_mul(sc2,mat4_mul(rot,liftM)));
                    trimesh_apply_transform(&job->trimesh,wmat);
                }
            }

            ufbx_free_scene(sc);
        }
        stream_job_compute_aabb(&req,job);
        job->valid=true;

        /* Push to ready queue */
        SDL_LockMutex(ss->mutex);
        if(ss->rdy_count<STREAM_QUEUE_CAP){
            ss->ready[ss->rdy_tail%STREAM_QUEUE_CAP]=job;
            ss->rdy_tail++;ss->rdy_count++;
        } else {
            /* Ready queue full — shouldn't happen, but free safely */
            stream_job_free(job);
        }
        SDL_UnlockMutex(ss->mutex);
    }
    return 0;
}

static void stream_init(StreamSystem *ss, PropList *pl){
    memset(ss,0,sizeof(*ss));
    ss->props=pl;
    ss->generation=1;
    ss->mutex=SDL_CreateMutex();
    ss->work_sem=SDL_CreateSemaphore(0);
    printf("[Stream] Init -> %s props=%d\n",
           scene_name(scene_id_for_props(pl)),
           pl?pl->count:0);
    ss->thread=SDL_CreateThread(stream_worker_thread,"PropStream",ss);
}

static void stream_shutdown(StreamSystem *ss){
    ss->quit=true;
    SDL_SemPost(ss->work_sem);
    int ret;SDL_WaitThread(ss->thread,&ret);
    SDL_DestroyMutex(ss->mutex);
    SDL_DestroySemaphore(ss->work_sem);
}

/* Queue a prop for background loading */
static void stream_request(StreamSystem *ss, int prop_idx){
    Prop *p=&ss->props->list[prop_idx];
    if(p->use_scene_renderer||p->stream_state!=PROP_UNLOADED) return;
    SDL_LockMutex(ss->mutex);
    if(ss->req_count<STREAM_QUEUE_CAP){
        StreamPropRequest *req=&ss->req[ss->req_tail%STREAM_QUEUE_CAP];
        memset(req,0,sizeof(*req));
        req->prop_idx=prop_idx;
        req->prop_uid=p->uid;
        req->generation=ss->generation;
        req->pos=p->pos;
        req->rot_x=p->rot_x;
        req->rot_y=p->rot_y;
        req->rot_z=p->rot_z;
        req->scale=p->scale;
        req->coll_type=p->coll_type;
        req->aabb_cx=p->aabb_cx;
        req->aabb_cz=p->aabb_cz;
        req->aabb_hw=p->aabb_hw;
        req->aabb_hd=p->aabb_hd;
        memcpy(req->path,p->path,sizeof(req->path));
        ss->req_tail++;ss->req_count++;
        p->stream_state=PROP_QUEUED;
        SDL_SemPost(ss->work_sem);
    }
    SDL_UnlockMutex(ss->mutex);
}

static void stream_prepare_job(Prop *p, StreamJob *job){
    p->num_meshes=0;
    p->ymin=job->ymin;
    p->bounds_min=job->bounds_min;
    p->bounds_max=job->bounds_max;
    p->trimesh=job->trimesh; /* transfer ownership */
    memset(&job->trimesh,0,sizeof(job->trimesh));

    if(p->coll_type==PROP_COLL_AABB){
        p->aabb=job->aabb;
    }
    static_collision_world_mark_dirty();
}

static bool stream_gpu_upload_step(Prop *p, StreamJob *job){
    if(job->upload_mesh>=job->num_meshes) return true;
    {
        CpuMeshData *cm=&job->meshes[job->upload_mesh];
        GpuMesh *gm=&p->meshes[p->num_meshes];
        memset(gm,0,sizeof(*gm));
        GLH_Attrib attrs[]={
            {0,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,pos)},
            {1,3,GL_FLOAT,GL_FALSE,offsetof(Vertex,normal)},
            {2,2,GL_FLOAT,GL_FALSE,offsetof(Vertex,uv)},
            {3,4,GL_FLOAT,GL_FALSE,offsetof(Vertex,weights)},
            {4,4,GL_INT,0,offsetof(Vertex,bones)}
        };
        gm->vao=GLH_mesh_full(cm->verts,cm->vc*sizeof(Vertex),
                              cm->inds,cm->ic*sizeof(uint32_t),
                              sizeof(Vertex),attrs,5,
                              &gm->vbo,&gm->ebo);
        gm->index_count=(GLsizei)cm->ic;
        gm->geometry_to_world=cm->geometry_to_world;
        gm->node_typed_id=cm->node_typed_id;
        memcpy(gm->cluster_map,cm->cluster_map,sizeof(gm->cluster_map));
        gm->num_gpu_bones=cm->num_gpu_bones;
        gm->mat_color[0]=cm->mat_color[0];
        gm->mat_color[1]=cm->mat_color[1];
        gm->mat_color[2]=cm->mat_color[2];
        gm->mat_color[3]=cm->mat_color[3];
        gm->alpha_cutoff=cm->alpha_cutoff;
        if(cm->tex_mem){
            gm->texture=GLH_texture_rgba_srgb_streamed(cm->tex_mem,cm->tex_w,cm->tex_h);
            stbi_image_free(cm->tex_mem);cm->tex_mem=NULL;
        } else if(cm->tex_path[0]){
            gm->texture=GLH_texture_file_srgb_streamed(cm->tex_path);
        }
        free(cm->verts);cm->verts=NULL;
        free(cm->inds);cm->inds=NULL;
        p->num_meshes++;
        job->upload_mesh++;
    }
    return job->upload_mesh>=job->num_meshes;
}

static void prop_cancel_ready_job(Prop *p){
    if(!p||!p->ready_job) return;
    stream_job_free(p->ready_job);
    p->ready_job=NULL;
}

/* Free GPU data for one prop, keep definition for future reload */
static void prop_unload_gpu(Prop *p){
    prop_cancel_ready_job(p);
    if(p->use_scene_renderer){
        trimesh_free(&p->trimesh);
        p->stream_state=PROP_LOADED;
        return;
    }
    if(p->static_asset_handle){
        static_mesh_cache_release(p->static_asset_handle);p->static_asset_handle=0;
        trimesh_free(&p->trimesh);p->num_meshes=0;p->stream_state=PROP_UNLOADED;static_collision_world_mark_dirty();return;
    }
    GLuint seen[MAX_MESHES*4];int seen_count=0;
    for(int mi=0;mi<p->num_meshes;mi++){
        glDeleteVertexArrays(1,&p->meshes[mi].vao);
        if(p->meshes[mi].vbo) glDeleteBuffers(1,&p->meshes[mi].vbo);
        if(p->meshes[mi].ebo) glDeleteBuffers(1,&p->meshes[mi].ebo);
        GLuint textures[4]={p->meshes[mi].texture,p->meshes[mi].normal_tex,p->meshes[mi].mr_tex,p->meshes[mi].emissive_tex};
        for(int ti=0;ti<4;ti++){
            GLuint texture=textures[ti];bool duplicate=false;
            if(texture_cache_release(texture))continue;
            for(int j=0;j<seen_count;j++) if(seen[j]==texture) duplicate=true;
            if(texture&&!duplicate){seen[seen_count++]=texture;glDeleteTextures(1,&texture);}
        }
    }
    trimesh_free(&p->trimesh);
    p->num_meshes=0;
    p->stream_state=PROP_UNLOADED;
    static_collision_world_mark_dirty();
}

static void prop_release_all(Prop *p){
    if(!p) return;
    prop_cancel_ready_job(p);
    if(p->use_scene_renderer) scene_free(&p->scene);
    else if(p->stream_state==PROP_LOADED) prop_unload_gpu(p);
    else trimesh_free(&p->trimesh);
    memset(&p->scene,0,sizeof(p->scene));
    p->num_meshes=0;
    p->use_scene_renderer=false;
    p->stream_state=PROP_UNLOADED;
    p->active=false;
    props_visibility_invalidate();
    static_collision_world_mark_dirty();
}

/* Per-frame streaming update — call from main loop */
void props_stream_update(PropList *pl, Vec3 cam){
    /* 1. Drain the ready queue — upload to GPU (max STREAM_GPU_PER_FRAME) */
    int uploads=0;
    Uint64 upload_begin=SDL_GetPerformanceCounter();
    while(uploads<STREAM_GPU_PER_FRAME){
        StreamJob *job=NULL;
        Prop *target=NULL;

        for(int i=0;i<pl->count;i++){
            if(pl->list[i].active&&pl->list[i].ready_job){
                target=&pl->list[i];
                break;
            }
        }
        if(!target){
            SDL_LockMutex(g_stream.mutex);
            if(g_stream.rdy_count>0){
                job=g_stream.ready[g_stream.rdy_head%STREAM_QUEUE_CAP];
                g_stream.rdy_head++;g_stream.rdy_count--;
            }
            SDL_UnlockMutex(g_stream.mutex);
            if(job){
                if(job->generation==g_stream.generation&&job->prop_idx>=0&&job->prop_idx<pl->count){
                    target=&pl->list[job->prop_idx];
                    if(target->active&&target->uid==job->prop_uid){
                        target->ready_job=job;
                        stream_prepare_job(target,job);
                        target->stream_state=PROP_UPLOADING;
                    }else{
                        stream_job_free(job);
                        job=NULL;
                    }
                }else{
                    stream_job_free(job);
                    job=NULL;
                }
            }
        }
        if(!target||!target->ready_job)break;
        if(stream_gpu_upload_step(target,target->ready_job)){
            stream_job_free(target->ready_job);
            target->ready_job=NULL;
            target->stream_state=PROP_LOADED;
            static_collision_world_mark_dirty();
        }
        uploads++;
    }
    g_props_stream_last_uploads=uploads;
    g_props_stream_last_upload_ms=(uploads>0)
        ? (float)((double)(SDL_GetPerformanceCounter()-upload_begin)*1000.0/
                  (double)SDL_GetPerformanceFrequency())
        : 0.0f;

    /* 2. Scan all registered props — load/unload based on distance */
    for(int i=0;i<pl->count;i++){
        Prop *p=&pl->list[i];
        if(!p->active)continue;
        if(prop_uses_scene_renderer(p))continue;
        float dx=p->pos.x-cam.x, dz=p->pos.z-cam.z;
        float d2=dx*dx+dz*dz;

        if(d2<=STREAM_LOAD_DIST*STREAM_LOAD_DIST){
            /* Want loaded */
            if(p->stream_state==PROP_UNLOADED)
                stream_request(&g_stream,i);
        } else if(d2>STREAM_UNLOAD_DIST*STREAM_UNLOAD_DIST){
            /* Want unloaded */
            if(p->stream_state==PROP_LOADED||p->stream_state==PROP_UPLOADING||p->ready_job)
                prop_unload_gpu(p);
            /* If QUEUED — it will finish loading and sit READY/LOADED briefly
               before the next frame unloads it.  Small waste but safe. */
        }
    }
}
static GLuint prop_make_prog(void){
    return GLH_program_f("shaders/static_pbr.vs.glsl","shaders/static_pbr.fs.glsl");
}
static void prop_cache_prog_uniforms(GLuint prog){
    g_prop_loc_model=glGetUniformLocation(prog,"uModel");
    g_prop_loc_albedo=glGetUniformLocation(prog,"uAlbedo");
    g_prop_loc_has_texture=glGetUniformLocation(prog,"uHasTexture");
    g_prop_loc_base_color=glGetUniformLocation(prog,"uBaseColor");
    g_prop_loc_alpha_cutoff=glGetUniformLocation(prog,"uAlphaCutoff");
    g_prop_loc_cam_pos=glGetUniformLocation(prog,"uCamPos");
    g_prop_loc_normal=glGetUniformLocation(prog,"uNormal");g_prop_loc_mr=glGetUniformLocation(prog,"uMR");g_prop_loc_emissive=glGetUniformLocation(prog,"uEmissive");
    g_prop_loc_has_normal=glGetUniformLocation(prog,"uHasNormal");g_prop_loc_has_mr=glGetUniformLocation(prog,"uHasMR");g_prop_loc_has_emissive=glGetUniformLocation(prog,"uHasEmissive");
    g_prop_loc_metallic=glGetUniformLocation(prog,"uMetallic");g_prop_loc_roughness=glGetUniformLocation(prog,"uRoughness");g_prop_loc_emissive_factor=glGetUniformLocation(prog,"uEmissiveFactor");
    g_prop_loc_fog_density=glGetUniformLocation(prog,"uFogDensity");g_prop_loc_fog_color=glGetUniformLocation(prog,"uFogColor");
}

static Mat4 prop_world(Prop *p){
    float d2r=(float)M_PI/180.0f;
    /* Ground-snap: translate by the unscaled source-space floor offset
       before applying object scale so growing a prop does not launch it upward. */
    Mat4 lift =mat4_translate(0,-p->ymin,0);
    Mat4 sc   =mat4_scale_xyz(p->scale,p->scale,p->scale);
    Mat4 rx   =mat4_rot_x(p->rot_x*d2r);
    Mat4 ry   =mat4_rot_y(p->rot_y*d2r);
    Mat4 rz   =mat4_rot_z(p->rot_z*d2r);
    Mat4 rot  =mat4_mul(ry,mat4_mul(rx,rz));
    Mat4 trans=mat4_translate(p->pos.x,p->pos.y,p->pos.z);
    return mat4_mul(trans,mat4_mul(sc,mat4_mul(rot,lift)));
}

static int props_next_slot(PropList *pl){
    if(!pl) return -1;
    for(int i=0;i<pl->count;i++){
        if(!pl->list[i].active) return i;
    }
    if(pl->count>=MAX_PROPS) return -1;
    return pl->count;
}

static float prop_asset_scale_override(const char *path){
    if(!path||!path[0]) return 1.0f;
    if(path_contains_ci(path,"models/shire/meshy_ai_a_low_poly_male_hobbi_biped/meshy_ai_a_low_poly_male_hobbi_biped_meshy_ai_meshy_merged_animations.glb")) return 0.01f;
    return 1.0f;
}

/* Register a prop definition — no loading, no GPU work.
   The streaming system loads it when the player gets close. */
static bool prop_register(PropList *pl,const char *fbx_path,
                       float wx,float wy,float wz,
                       float rx,float ry,float rz,float scale,
                       PropCollType coll,
                       float aabb_cx,float aabb_cz,
                       float aabb_hw,float aabb_hd){
    if(prop_path_is_likely_skinned(fbx_path)||
       path_has_extension_ci(fbx_path,".gltf")||
       path_has_extension_ci(fbx_path,".glb"))
        return prop_place(pl,fbx_path,wx,wy,wz,rx,ry,rz,scale,coll,aabb_cx,aabb_cz,aabb_hw,aabb_hd);
    int idx=props_next_slot(pl);
    if(idx<0){fprintf(stderr,"[Prop] MAX_PROPS reached\n");return false;}
    Prop *p=&pl->list[idx];memset(p,0,sizeof(Prop));
    p->uid=world_alloc_prop_uid();
    strncpy(p->path,fbx_path,255);
    {
        float asset_scale=prop_asset_scale_override(fbx_path);
        if(asset_scale!=1.0f) scale*=asset_scale;
    }
    p->rot_x=rx;p->rot_y=ry;p->rot_z=rz;
    p->scale=(scale<=0.0f)?1.0f:scale;
    p->pos.x=wx;p->pos.y=(wy==0.0f)?scene_ground_y(wx,wz):wy;p->pos.z=wz;
    p->coll_type=coll;
    p->aabb_cx=aabb_cx;p->aabb_cz=aabb_cz;
    p->aabb_hw=aabb_hw;p->aabb_hd=aabb_hd;
    p->stream_state=PROP_UNLOADED;
    p->pickup_item=ITEM_NONE;
    p->pickup_count=1;
    prop_init_interaction_defaults(p);
    p->active=true;
    if(idx==pl->count) pl->count++;
    props_visibility_invalidate();
    return true;
}

/* Legacy prop_place: immediately loads and uploads to GPU.
   Only used for props that MUST be present at game start
   (e.g., collision-critical objects near spawn).           */
static bool prop_place(PropList *pl,const char *fbx_path,
                       float wx,float wy,float wz,
                       float rx,float ry,float rz,float scale,
                       PropCollType coll,
                       float aabb_cx,float aabb_cz,
                       float aabb_hw,float aabb_hd){
    int idx=props_next_slot(pl);
    if(idx<0){fprintf(stderr,"[Prop] MAX_PROPS reached\n");return false;}
    Prop *p=&pl->list[idx];memset(p,0,sizeof(Prop));
    p->uid=world_alloc_prop_uid();
    strncpy(p->path,fbx_path,255);
    {
        float asset_scale=prop_asset_scale_override(fbx_path);
        if(asset_scale!=1.0f) scale*=asset_scale;
    }
    if(path_has_extension_ci(fbx_path,".gltf")||path_has_extension_ci(fbx_path,".glb")){
        Scene tmp={0};
        float xmn=1e9f,ymn=1e9f,zmn=1e9f,xmx=-1e9f,ymx=-1e9f,zmx=-1e9f;
        if(!static_mesh_cache_load(fbx_path,&tmp,&p->static_asset_handle)){fprintf(stderr,"[Prop] Load '%s': glTF failed\n",fbx_path);return false;}
        if(prop_scene_needs_scene_renderer(&tmp)){
            p->scene=tmp;
            memset(&tmp,0,sizeof(tmp));
            p->use_scene_renderer=true;
            p->ymin=p->scene.ymin_raw;
            p->rot_x=rx; p->rot_y=ry; p->rot_z=rz;
            p->scale=(scale<=0.0f)?1.0f:scale;
            p->pos.x=wx; p->pos.y=(wy==0.0f)?scene_ground_y(wx,wz):wy; p->pos.z=wz;
            p->coll_type=coll;
            p->aabb_cx=aabb_cx; p->aabb_cz=aabb_cz;
            p->aabb_hw=aabb_hw; p->aabb_hd=aabb_hd;
            p->pickup_item=ITEM_NONE;
            p->pickup_count=1;
            prop_init_interaction_defaults(p);
            p->bounds_min=(Vec3){-1.0f,p->scene.ymin_raw,-1.0f};
            p->bounds_max=(Vec3){ 1.0f,p->scene.ymax_raw, 1.0f};
            if(coll==PROP_COLL_AABB){
                prop_rebuild_aabb_from_bounds(p);
            }else if(coll==PROP_COLL_MESH){
                if(p->scene.scene) trimesh_build(&p->trimesh,p->scene.scene);
                else if(p->scene.gltf) trimesh_build_gltf(&p->trimesh,p->scene.gltf);
                trimesh_apply_transform(&p->trimesh,prop_world(p));
            }
            p->active=true;
            p->stream_state=PROP_LOADED;
            if(idx==pl->count) pl->count++;
            props_visibility_invalidate();
            static_collision_world_mark_dirty();
            printf("[Prop] '%s' using scene renderer at (%.1f,%.1f,%.1f)\n",
                   fbx_path,(double)wx,(double)p->pos.y,(double)wz);
            return true;
        }
        p->num_meshes=tmp.num_meshes;
        for(int i=0;i<tmp.num_meshes;i++)p->meshes[i]=tmp.meshes[i];
        tmp.num_meshes=0;
        p->ymin=tmp.ymin_raw;
        p->rot_x=rx; p->rot_y=ry; p->rot_z=rz;
        p->scale=(scale<=0.0f)?1.0f:scale;
        p->pos.x=wx; p->pos.y=(wy==0.0f)?scene_ground_y(wx,wz):wy; p->pos.z=wz;
        p->coll_type=coll;
        p->pickup_item=ITEM_NONE;
        p->pickup_count=1;
        prop_init_interaction_defaults(p);
        if(p->static_asset_handle){
            StaticMeshAsset *asset=&g_static_mesh_assets[p->static_asset_handle-1];
            xmn=asset->min.x;ymn=asset->min.y;zmn=asset->min.z;xmx=asset->max.x;ymx=asset->max.y;zmx=asset->max.z;
        }else{
        for(size_t ni=0;ni<tmp.gltf->nodes_count;ni++){
            cgltf_node *nd=&tmp.gltf->nodes[ni];if(!nd->mesh)continue;
            {
                cgltf_float nworld[16];
                cgltf_node_transform_world(nd,nworld);
                Mat4 g2w=mat4_from_cgltf(nworld);
                for(size_t pi=0;pi<nd->mesh->primitives_count;pi++){
                    const cgltf_attribute *pos_attr=gltf_find_attribute(&nd->mesh->primitives[pi],cgltf_attribute_type_position,0);
                    if(!pos_attr||!pos_attr->data)continue;
                    for(size_t vi=0;vi<pos_attr->data->count;vi++){
                        cgltf_float lp[3]={0};
                        cgltf_accessor_read_float(pos_attr->data,vi,lp,3);
                        {
                            float tx=g2w.m[0]*lp[0]+g2w.m[4]*lp[1]+g2w.m[8]*lp[2]+g2w.m[12];
                            float ty=g2w.m[1]*lp[0]+g2w.m[5]*lp[1]+g2w.m[9]*lp[2]+g2w.m[13];
                            float tz=g2w.m[2]*lp[0]+g2w.m[6]*lp[1]+g2w.m[10]*lp[2]+g2w.m[14];
                            if(tx<xmn)xmn=tx;if(tx>xmx)xmx=tx;
                            if(ty<ymn)ymn=ty;if(ty>ymx)ymx=ty;
                            if(tz<zmn)zmn=tz;if(tz>zmx)zmx=tz;
                        }
                    }
                }
            }
        }
        }
        p->bounds_min=(Vec3){xmn,ymn,zmn};
        p->bounds_max=(Vec3){xmx,ymx,zmx};
        if(coll==PROP_COLL_AABB){
            if(aabb_hw>0.0f){
                float h=(tmp.ymax_raw-tmp.ymin_raw)*p->scale;
                p->aabb.min=(Vec3){wx+aabb_cx-aabb_hw,p->pos.y,wz+aabb_cz-aabb_hd};
                p->aabb.max=(Vec3){wx+aabb_cx+aabb_hw,p->pos.y+h,wz+aabb_cz+aabb_hd};
            }else{
                prop_rebuild_aabb_from_bounds(p);
            }
        }else if(coll==PROP_COLL_MESH){
            trimesh_build_gltf(&p->trimesh,tmp.gltf);
            trimesh_apply_transform(&p->trimesh,prop_world(p));
        }
        p->active=true;
        p->stream_state=PROP_LOADED;
        if(p->static_asset_handle) memset(&tmp,0,sizeof(tmp));
        else scene_free(&tmp);
        if(idx==pl->count) pl->count++;
        props_visibility_invalidate();
        static_collision_world_mark_dirty();
        printf("[Prop] '%s' at (%.1f,%.1f,%.1f) rot=(%.1f,%.1f,%.1f) scale=%.2f coll=%d\n",
               fbx_path,(double)wx,(double)p->pos.y,(double)wz,(double)rx,(double)ry,(double)rz,(double)p->scale,coll);
        return true;
    }
    ufbx_load_opts opts={0};opts.generate_missing_normals=true;
    opts.target_axes=ufbx_axes_right_handed_y_up;opts.target_unit_meters=1.0f;
    ufbx_error err;ufbx_scene *sc=ufbx_load_file(fbx_path,&opts,&err);
    if(!sc){fprintf(stderr,"[Prop] Load '%s': %s\n",fbx_path,err.description.data);return false;}
    /* GPU meshes */
    ufbx_skin_deformer *ds=NULL;
    for(size_t ni=0;ni<sc->nodes.count&&p->num_meshes<MAX_MESHES;ni++){
        ufbx_node *nd=sc->nodes.data[ni];if(!nd->mesh)continue;
        size_t mat_count=nd->mesh->materials.count;
        bool split_by_material=mat_count>1&&nd->mesh->face_material.count==nd->mesh->num_faces;
        size_t passes=split_by_material?mat_count:1;
        for(size_t part=0;part<passes&&p->num_meshes<MAX_MESHES;part++){
            if(build_gpu_mesh(&p->meshes[p->num_meshes],&ds,nd->mesh,nd,fbx_path,split_by_material?part:0))p->num_meshes++;
        }
    }
    /* Local Y bounds for Y-flip (needed for render transform) */
    float xmn=1e9f,ymn=1e9f,zmn=1e9f,xmx=-1e9f,ymx=-1e9f,zmx=-1e9f;
    for(size_t ni=0;ni<sc->nodes.count;ni++){
        ufbx_node *nd=sc->nodes.data[ni];if(!nd->mesh)continue;
        for(size_t fi=0;fi<nd->mesh->num_faces;fi++){
            ufbx_face face=nd->mesh->faces.data[fi];
            for(size_t i=face.index_begin;i<face.index_begin+face.num_indices;i++){
                ufbx_vec3 lp=ufbx_get_vertex_vec3(&nd->mesh->vertex_position,i);
                ufbx_vec3 wp=ufbx_transform_position(&nd->geometry_to_world,lp);
                float x=(float)wp.x;
                float y=(float)wp.y;
                float z=(float)wp.z;
                if(x<xmn)xmn=x;if(x>xmx)xmx=x;
                if(y<ymn)ymn=y;if(y>ymx)ymx=y;
                if(z<zmn)zmn=z;if(z>zmx)zmx=z;
            }
        }
    }
    p->ymin=ymn;
    p->bounds_min=(Vec3){xmn,ymn,zmn};
    p->bounds_max=(Vec3){xmx,ymx,zmx};
    p->rot_x=rx; p->rot_y=ry; p->rot_z=rz;
    p->scale=(scale<=0.0f)?1.0f:scale;
    p->pos.x=wx; p->pos.y=(wy==0.0f)?scene_ground_y(wx,wz):wy; p->pos.z=wz;
    p->coll_type=coll;
    p->pickup_item=ITEM_NONE;
    p->pickup_count=1;
    prop_init_interaction_defaults(p);
    if(coll==PROP_COLL_AABB){
        if(aabb_hw>0.0f){
            /* Manual override */
            float h=(-ymn+ymx)*p->scale;
            p->aabb.min=(Vec3){wx+aabb_cx-aabb_hw, p->pos.y,           wz+aabb_cz-aabb_hd};
            p->aabb.max=(Vec3){wx+aabb_cx+aabb_hw, p->pos.y+h,         wz+aabb_cz+aabb_hd};
        } else {
            /* Use prop_world(p) * nd->geometry_to_world per node — same as props_draw */
            Mat4 world=prop_world(p);
            float wxmn=1e9f,wymn=1e9f,wzmn=1e9f,wxmx=-1e9f,wymx=-1e9f,wzmx=-1e9f;
            for(size_t ni=0;ni<sc->nodes.count;ni++){
                ufbx_node *nd=sc->nodes.data[ni];if(!nd->mesh)continue;
                Mat4 g2w=mat4_from_ufbx(&nd->geometry_to_world);
                Mat4 M=mat4_mul(world,g2w);
                for(size_t fi=0;fi<nd->mesh->num_faces;fi++){
                    ufbx_face face=nd->mesh->faces.data[fi];
                    for(size_t i=face.index_begin;i<face.index_begin+face.num_indices;i++){
                        ufbx_vec3 lp=ufbx_get_vertex_vec3(&nd->mesh->vertex_position,i);
                        float vx=(float)lp.x,vy=(float)lp.y,vz=(float)lp.z;
                        float tx=M.m[0]*vx+M.m[4]*vy+M.m[8]*vz+M.m[12];
                        float ty=M.m[1]*vx+M.m[5]*vy+M.m[9]*vz+M.m[13];
                        float tz=M.m[2]*vx+M.m[6]*vy+M.m[10]*vz+M.m[14];
                        if(tx<wxmn)wxmn=tx;if(tx>wxmx)wxmx=tx;
                        if(ty<wymn)wymn=ty;if(ty>wymx)wymx=ty;
                        if(tz<wzmn)wzmn=tz;if(tz>wzmx)wzmx=tz;
                    }
                }
            }
            p->aabb.min=(Vec3){wxmn,wymn,wzmn};
            p->aabb.max=(Vec3){wxmx,wymx,wzmx};
        }
    } else if(coll==PROP_COLL_MESH){
        trimesh_build(&p->trimesh,sc);
        trimesh_apply_transform(&p->trimesh,prop_world(p));
    }
    p->active=true;
    p->stream_state=PROP_LOADED;  /* already on GPU — don't re-stream */
    ufbx_free_scene(sc);
    if(idx==pl->count) pl->count++;
    props_visibility_invalidate();
    static_collision_world_mark_dirty();
    printf("[Prop] '%s' at (%.1f,%.1f,%.1f) rot=(%.1f,%.1f,%.1f) scale=%.2f coll=%d\n",
           fbx_path,(double)wx,(double)p->pos.y,(double)wz,
           (double)rx,(double)ry,(double)rz,(double)p->scale,coll);
    return true;
}

static void props_draw(PropList *pl,SkinProg *skin,LightingSystem *ls,Mat4 proj,Mat4 view,Vec3 cam,float fog_density,Vec3 fog_color){
    if(!pl->count||!g_prop_prog)return;
    Mat4 pv=mat4_mul(proj,view);
    float planes[6][4];
    const float cell_draw_limit=520.0f;
    frustum_extract(planes,pv);
    props_visibility_rebuild(pl);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    lighting_push(ls,g_prop_prog);GLH_begin(g_prop_prog,proj.m,view.m);
    if(g_prop_loc_cam_pos>=0) glUniform3f(g_prop_loc_cam_pos,cam.x,cam.y,cam.z);
    glUniform1f(g_prop_loc_fog_density,fog_density);
    glUniform3f(g_prop_loc_fog_color,fog_color.x,fog_color.y,fog_color.z);
    for(int ci=0;ci<g_prop_vis_cache.cell_count;ci++){
        const PropVisCell *cell=&g_prop_vis_cache.cells[ci];
        if(!prop_vis_cell_frustum_visible(planes,cell)) continue;
        if(prop_vis_cell_dist2_xz(cell,cam)>cell_draw_limit*cell_draw_limit) continue;
        for(int pi=cell->head;pi>=0;pi=g_prop_vis_cache.next_prop[pi]){
            Prop *p=&pl->list[pi];
            float cull_r,cull_y;
            if(!p->active||p->stream_state!=PROP_LOADED)continue;
            float dx=p->pos.x-cam.x, dz=p->pos.z-cam.z;
            float d2=dx*dx+dz*dz;
            float lod_r=prop_render_distance(p);
            if(d2>lod_r*lod_r)continue;
            cull_r=prop_cull_radius(p);
            cull_y=prop_cull_center_y(p);
            if(!frustum_sphere(planes,p->pos.x,cull_y,p->pos.z,cull_r))continue;
            if(prop_uses_scene_renderer(p)){
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
                draw_scene_prop(skin,&p->scene,prop_world(p),proj,view,cam,ls);
                lighting_push(ls,g_prop_prog);
                GLH_begin(g_prop_prog,proj.m,view.m);
                if(g_prop_loc_cam_pos>=0) glUniform3f(g_prop_loc_cam_pos,cam.x,cam.y,cam.z);
                continue;
            }
            Mat4 world=prop_world(p);
            for(int mi=0;mi<p->num_meshes;mi++){GpuMesh *gm=&p->meshes[mi];
                Mat4 mesh_mat=mat4_mul(world,gm->geometry_to_world);
                if(prop_mesh_is_two_sided(gm)) glDisable(GL_CULL_FACE);
                else glEnable(GL_CULL_FACE);
                glUniformMatrix4fv(g_prop_loc_model,1,GL_FALSE,mesh_mat.m);
                if(gm->texture){
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D,gm->texture);
                    glUniform1i(g_prop_loc_albedo,0);
                    glUniform1i(g_prop_loc_has_texture,1);
                }else{
                    glUniform1i(g_prop_loc_has_texture,0);
                }
                glUniform4f(g_prop_loc_base_color,gm->mat_color[0],gm->mat_color[1],gm->mat_color[2],gm->mat_color[3]);
                glUniform1f(g_prop_loc_alpha_cutoff,gm->alpha_cutoff);
                glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,gm->normal_tex);glUniform1i(g_prop_loc_normal,1);glUniform1i(g_prop_loc_has_normal,gm->normal_tex!=0);
                glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,gm->mr_tex);glUniform1i(g_prop_loc_mr,2);glUniform1i(g_prop_loc_has_mr,gm->mr_tex!=0);
                glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,gm->emissive_tex);glUniform1i(g_prop_loc_emissive,3);glUniform1i(g_prop_loc_has_emissive,gm->emissive_tex!=0);
                glUniform1f(g_prop_loc_metallic,gm->metallic);glUniform1f(g_prop_loc_roughness,gm->roughness);
                glUniform3fv(g_prop_loc_emissive_factor,1,gm->emissive);
                glBindVertexArray(gm->vao);glDrawElements(GL_TRIANGLES,gm->index_count,GL_UNSIGNED_INT,0);}
        }
    }
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}
void props_shadow(PropList *pl,ShadowMap *sm,LightingSystem *ls,Vec3 cam){
    if(!pl->count)return;
    float planes[6][4];
    const float cell_shadow_limit=g_render_shadow_pass_wide?160.0f:180.0f;
    glUseProgram(sm->prog_depth);
    glUniformMatrix4fv(glGetUniformLocation(sm->prog_depth,"uLightVP"),1,GL_FALSE,ls->light_vp.m);
    frustum_extract(planes,ls->light_vp);
    props_visibility_rebuild(pl);
    for(int ci=0;ci<g_prop_vis_cache.cell_count;ci++){
        const PropVisCell *cell=&g_prop_vis_cache.cells[ci];
        if(!prop_vis_cell_frustum_visible(planes,cell)) continue;
        if(prop_vis_cell_dist2_xz(cell,cam)>cell_shadow_limit*cell_shadow_limit) continue;
        for(int pi=cell->head;pi>=0;pi=g_prop_vis_cache.next_prop[pi]){Prop *p=&pl->list[pi];
            float cull_r,cull_y;
            float dx,dz,cam_dist2;
            if(!p->active||p->stream_state!=PROP_LOADED)continue;
            dx=p->pos.x-cam.x;
            dz=p->pos.z-cam.z;
            cam_dist2=dx*dx+dz*dz;
            if(!prop_should_cast_shadow(p,g_render_shadow_pass_wide,cam_dist2)) continue;
            cull_r=prop_cull_radius(p);
            cull_y=prop_cull_center_y(p);
            if(!frustum_sphere(planes,p->pos.x,cull_y,p->pos.z,cull_r)) continue;
            if(prop_uses_scene_renderer(p)){
                shadow_scene_prop(sm,ls,&p->scene,prop_world(p));
                glUseProgram(sm->prog_depth);
                glUniformMatrix4fv(glGetUniformLocation(sm->prog_depth,"uLightVP"),1,GL_FALSE,ls->light_vp.m);
                continue;
            }
            Mat4 world=prop_world(p);
            for(int mi=0;mi<p->num_meshes;mi++){
                Mat4 mesh_mat=mat4_mul(world,p->meshes[mi].geometry_to_world);
                glUniformMatrix4fv(glGetUniformLocation(sm->prog_depth,"uModel"),1,GL_FALSE,mesh_mat.m);
                GpuMesh *gm=&p->meshes[mi];
                glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,gm->texture);
                glUniform1i(glGetUniformLocation(sm->prog_depth,"uAlphaTexture"),0);
                glUniform1f(glGetUniformLocation(sm->prog_depth,"uAlphaCutoff"),gm->texture?gm->alpha_cutoff:0.0f);
                glUniform1f(glGetUniformLocation(sm->prog_depth,"uBaseAlpha"),gm->mat_color[3]);
                if(prop_mesh_is_two_sided(gm))glDisable(GL_CULL_FACE);else glEnable(GL_CULL_FACE);
                glBindVertexArray(p->meshes[mi].vao);glDrawElements(GL_TRIANGLES,p->meshes[mi].index_count,GL_UNSIGNED_INT,0);}
        }
    }glBindVertexArray(0);glEnable(GL_CULL_FACE);
}
static void props_free(PropList *pl){
    for(int pi=0;pi<pl->count;pi++){Prop *p=&pl->list[pi];
        if(!p->active)continue;
        prop_release_all(p);
    }
    props_visibility_invalidate();
}

void props_resolve_capsule(PropList *pl, Vec3 *pos, Vec3 *vel, bool *on_ground,
                           float height, float radius){
    for(int pi=0;pi<pl->count;pi++){
        Prop *p=&pl->list[pi];
        if(!p->active||p->stream_state!=PROP_LOADED)continue;
        if(p->coll_type==PROP_COLL_AABB){
            AABB *b=&p->aabb;
            float py=pos->y,ptop=py+height;
            if(py>b->max.y+0.05f||ptop<b->min.y-0.05f)continue;
            if(pos->x+radius<b->min.x||pos->x-radius>b->max.x)continue;
            if(pos->z+radius<b->min.z||pos->z-radius>b->max.z)continue;
            bool over_box=(pos->x>b->min.x-radius&&pos->x<b->max.x+radius&&
                           pos->z>b->min.z-radius&&pos->z<b->max.z+radius);
            if(over_box&&py<=b->max.y+0.25f&&py>=b->max.y-0.5f&&vel->y<=0.0f){
                pos->y=b->max.y; vel->y=0.0f; *on_ground=true; continue;
            }
            float left=pos->x+radius-b->min.x, right=b->max.x-(pos->x-radius);
            float front=pos->z+radius-b->min.z, back=b->max.z-(pos->z-radius);
            if(left<0||right<0||front<0||back<0)continue;
            float ox=left<right?-left:right;
            float oz=front<back?-front:back;
            if(fabsf(ox)<fabsf(oz))pos->x+=ox; else pos->z+=oz;
        } else if(p->coll_type==PROP_COLL_MESH){
            if(trimesh_resolve(&p->trimesh,pos,radius,height)){
                if(vel->y<=0.0f){ vel->y=0.0f; *on_ground=true; }
            }
        }
    }
}

/* FLOW-FIELD NAVMESH
   256×256 grid over the 5km×5km world.  BFS from player each
   time the player steps into a new grid cell.  Obstacles come
   from prop AABBs.  Enemies sample the grid for chase direction.
   ══════════════════════════════════════════════════════════════ */
typedef struct{
    uint8_t dir[NAV_H][NAV_W]; /* 0-7 = move dir, NAV_BLOCKED = wall/unvisited */
    int     pcx, pcz;          /* last player cell that triggered rebuild */
    bool    dirty;
}NavGrid;
static NavGrid g_nav;
static bool    g_nav_ready=false;

/* 8-direction deltas: E NE N NW W SW S SE */
static const int NAV_DX[8]={1,1,0,-1,-1,-1, 0, 1};
static const int NAV_DZ[8]={0,1,1, 1, 0,-1,-1,-1};
/* Opposite direction index */
static const uint8_t NAV_OPP[8]={4,5,6,7,0,1,2,3};
extern int static_collision_world_obstacle_count(void);
extern bool static_collision_world_obstacle_aabb(int idx,AABB *out_box);

static int nav_world_to_cx(float wx){ int c=(int)((wx+WORLD_HALF)/NAV_CELL); return c<0?0:c>=NAV_W?NAV_W-1:c; }
static int nav_world_to_cz(float wz){ int c=(int)((wz+WORLD_HALF)/NAV_CELL); return c<0?0:c>=NAV_H?NAV_H-1:c; }

static void nav_mark_aabb_cells(uint8_t obs[NAV_H][NAV_W],const AABB *box){
    float mnx,mxx,mnz,mxz;
    int x0,x1,z0,z1;
    if(!box) return;
    mnx=box->min.x-NAV_CELL; mxx=box->max.x+NAV_CELL;
    mnz=box->min.z-NAV_CELL; mxz=box->max.z+NAV_CELL;
    x0=nav_world_to_cx(mnx); x1=nav_world_to_cx(mxx);
    z0=nav_world_to_cz(mnz); z1=nav_world_to_cz(mxz);
    for(int zz=z0;zz<=z1;zz++) for(int xx=x0;xx<=x1;xx++) obs[zz][xx]=1;
}

/* Mark cells blocked by prop AABBs */
/* BFS flow-field rebuild — runs on main thread only, ~1ms for 256×256 */
static void nav_rebuild(Vec3 player_pos, PropList *pl){
    /* Two-step: build obstacle map, then BFS for flow directions.
       g_nav.dir stores the outgoing direction (0-7) toward player,
       or NAV_BLOCKED for walls.  Unvisited open cells are 0xFE     */
    static uint8_t obs[NAV_H][NAV_W];          /* obstacle map */
    memset(obs, 0, sizeof(obs));

    (void)pl;
    for(int si=0;si<static_collision_world_obstacle_count();si++){
        AABB box;
        if(!static_collision_world_obstacle_aabb(si,&box)) continue;
        nav_mark_aabb_cells(obs,&box);
    }

    /* Initialise direction map: walls=NAV_BLOCKED, open=0xFE (unvisited) */
    for(int z=0;z<NAV_H;z++)
        for(int x=0;x<NAV_W;x++)
            g_nav.dir[z][x]=obs[z][x]?NAV_BLOCKED:0xFE;

    /* BFS from player cell outward — stores "direction back to player" */
    static uint16_t qx[NAV_W*NAV_H], qz2[NAV_W*NAV_H];
    int head=0, tail=0;
    int sx=nav_world_to_cx(player_pos.x);
    int sz=nav_world_to_cz(player_pos.z);
    /* Force player cell open even if inside a prop */
    g_nav.dir[sz][sx]=0; /* sentinel: 'arrived' */
    qx[tail]=(uint16_t)sx; qz2[tail]=(uint16_t)sz;
    tail=(tail+1)%(NAV_W*NAV_H);

    while(head!=tail){
        int cx=(int)qx[head], cz=(int)qz2[head];
        head=(head+1)%(NAV_W*NAV_H);
        for(int d=0;d<8;d++){
            int nx=cx+NAV_DX[d], nz=cz+NAV_DZ[d];
            if(nx<0||nx>=NAV_W||nz<0||nz>=NAV_H) continue;
            if(g_nav.dir[nz][nx]!=0xFE) continue; /* already visited or wall */
            /* Direction stored = which way this cell should move to reach player */
            g_nav.dir[nz][nx]=NAV_OPP[d];
            qx[tail]=(uint16_t)nx; qz2[tail]=(uint16_t)nz;
            tail=(tail+1)%(NAV_W*NAV_H);
        }
    }
    /* Any cell still 0xFE is unreachable — mark blocked */
    for(int z=0;z<NAV_H;z++)
        for(int x=0;x<NAV_W;x++)
            if(g_nav.dir[z][x]==0xFE) g_nav.dir[z][x]=NAV_BLOCKED;

    g_nav.pcx=sx; g_nav.pcz=sz;
    g_nav.dirty=false;
    g_nav_ready=true;
}

void nav_rebuild_if_needed(Vec3 player_pos, PropList *pl){
    int pcx=nav_world_to_cx(player_pos.x);
    int pcz=nav_world_to_cz(player_pos.z);
    if(!g_nav_ready||pcx!=g_nav.pcx||pcz!=g_nav.pcz)
        nav_rebuild(player_pos,pl);
}

/* Sample flow field — returns move direction for an entity at pos.
   Falls back to direct-to-player dir if no path found.            */
static Vec3 nav_sample(Vec3 pos, Vec3 player_pos){
    if(!g_nav_ready){
        Vec3 d=vec3_sub(player_pos,pos); float l=vec3_len(d);
        return l>0.01f?vec3_scale(d,1.0f/l):(Vec3){0,0,0};
    }
    int cx=nav_world_to_cx(pos.x);
    int cz=nav_world_to_cz(pos.z);
    uint8_t dir=g_nav.dir[cz][cx];
    if(dir==NAV_BLOCKED||dir>=8){
        /* No path — try neighbours or direct fallback */
        Vec3 d=vec3_sub(player_pos,pos); float l=vec3_len(d);
        return l>0.01f?vec3_scale(d,1.0f/l):(Vec3){0,0,0};
    }
    /* Blend toward flow direction (smooth steering) */
    Vec3 flow={(float)NAV_DX[dir],0,(float)NAV_DZ[dir]};
    /* Weight by how close the flow points toward actual player */
    Vec3 direct=vec3_sub(player_pos,pos);
    float dl=vec3_len(direct);
    if(dl<0.01f)return flow;
    direct=vec3_scale(direct,1.0f/dl);
    /* Blend: close to player -> direct aim, far -> flow field */
    float blend=clampf(1.0f - dl/80.0f, 0.0f, 1.0f);
    Vec3 result={(flow.x*(1.0f-blend)+direct.x*blend),
                 0,
                 (flow.z*(1.0f-blend)+direct.z*blend)};
    float rl=sqrtf(result.x*result.x+result.z*result.z);
    return rl>0.001f?(Vec3){result.x/rl,0,result.z/rl}:(Vec3){0,0,0};
}

#define _POSIX_C_SOURCE 200809L
#define main valwood_game_main
#include "../main.c"
#undef main
#include <assert.h>
#include <sys/stat.h>

int main(void){
    char engine_root[1024],source[1400];
    assert(getcwd(engine_root,sizeof(engine_root)));
    snprintf(source,sizeof(source),"%s/projects/seldonne/models/sign.glb",engine_root);
    char directory[]="/tmp/valwood-studio-test-XXXXXX";
    assert(mkdtemp(directory)&&chdir(directory)==0);
    SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("Valwood integration",0,0,640,360,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    assert(window);SDL_GLContext context=SDL_GL_CreateContext(window);assert(context);
    char shader_path[1400];snprintf(shader_path,sizeof(shader_path),"%s/shaders",engine_root);assert(symlink(shader_path,"shaders")==0);
    const char *vertex[]={"shaders/static_pbr.vs.glsl","shaders/terrain_land.vs.glsl","shaders/grass.vs.glsl"};
    const char *fragment[]={"shaders/static_pbr.fs.glsl","shaders/terrain_land.fs.glsl","shaders/grass.fs.glsl"};
    for(int i=0;i<3;i++){GLuint shader=GLH_program_f(vertex[i],fragment[i]);GLint linked=0;glGetProgramiv(shader,GL_LINK_STATUS,&linked);assert(linked);glDeleteProgram(shader);}
    // The terrain must see newly painted grass masks, with exact authoring IDs.
    memset(g_grass_mask.d,255,sizeof(g_grass_mask.d));g_grass_mask_active=true;
    g_grass_coverage_dirty=true;
    GLuint coverage_tex=grass_coverage_texture();
    uint8_t *coverage=malloc(GRASS_MASK_RES*GRASS_MASK_RES*3);assert(coverage);
    glBindTexture(GL_TEXTURE_2D,coverage_tex);glGetTexImage(GL_TEXTURE_2D,0,GL_RGB,GL_UNSIGNED_BYTE,coverage);
    assert(coverage[0]==255&&coverage[1]==0&&coverage[2]==0);
    g_grass_mask_seeded=true;
    grass_mask_brush(-5,7,8,1,true,GRASS_STYLE_MEADOW,GRASS_TINT_NATURAL);
    assert(g_grass_coverage_dirty);grass_coverage_texture();assert(!g_grass_coverage_dirty);
    glGetTexImage(GL_TEXTURE_2D,0,GL_RGB,GL_UNSIGNED_BYTE,coverage);
    int mx=(int)((-5+WORLD_HALF)/(WORLD_HALF*2)*(GRASS_MASK_RES-1));
    int mz=(int)((7+WORLD_HALF)/(WORLD_HALF*2)*(GRASS_MASK_RES-1));
    assert(coverage[(mz*GRASS_MASK_RES+mx)*3]<8);
    free(coverage);glDeleteTextures(1,&g_grass_coverage_tex);g_grass_coverage_tex=0;
    g_grass_mask_active=g_grass_mask_seeded=false;g_grass_coverage_dirty=true;
    // Rebuilding terrain must release its vertex and index storage, not just the VAO.
    Terrain test_terrain={0};test_terrain.chunks[0].vao[0]=terrain_build_chunk_lod(0,0,4,&test_terrain.chunks[0].ic[0]);
    GLuint terrain_vao=test_terrain.chunks[0].vao[0];GLint terrain_vbo=0,terrain_ebo=0;
    glBindVertexArray(terrain_vao);glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&terrain_vbo);glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&terrain_ebo);glBindVertexArray(0);
    assert(glIsBuffer(terrain_vbo)&&glIsBuffer(terrain_ebo));terrain_chunk_release(&test_terrain.chunks[0]);
    assert(!glIsVertexArray(terrain_vao)&&!glIsBuffer(terrain_vbo)&&!glIsBuffer(terrain_ebo));
    g_scene_sys.current=SCENE_SHIRE;
    placer_init(&g_placer);
    char message[256];assert(editor_import_model(&g_placer,source,message,sizeof(message)));
    assert(g_placer.palette_count==1&&g_placer.filtered_count==1);
    char imported[256];snprintf(imported,sizeof(imported),"%s",g_placer.palette[0]);
    assert(!strcmp(imported,"imports/models/sign.glb"));
    FILE *bad=fopen("bad.glb","wb");fputs("invalid",bad);fclose(bad);
    assert(!editor_import_model(&g_placer,"bad.glb",message,sizeof(message)));
    EditorRuntimeState editor={0};EditorShellActionContext actions={0};actions.placer=&g_placer;actions.editor=&editor;
    snprintf(g_placer.search,sizeof(g_placer.search),"missing");editor_shell_browser_filter(&actions);assert(g_placer.filtered_count==0);
    snprintf(g_placer.search,sizeof(g_placer.search),"sign");editor_shell_browser_filter(&actions);assert(g_placer.filtered_count==1);
    assert(prop_register(&g_props,imported,5,3,7,0,0,0,1,PROP_COLL_AABB,0,0,0,0));
    assert(g_props.count==1&&g_props.list[0].active&&g_props.list[0].stream_state==PROP_LOADED);
    assert(g_props.list[0].static_asset_handle>0);
    GLuint shared_vao=g_props.list[0].meshes[0].vao,shared_texture=g_props.list[0].meshes[0].texture;
    Scene borrowed={0};int shared_handle=0;
    assert(static_mesh_cache_load(imported,&borrowed,&shared_handle));
    assert(shared_handle==g_props.list[0].static_asset_handle&&borrowed.meshes[0].vao==shared_vao);
    static_mesh_cache_release(shared_handle);assert(glIsVertexArray(shared_vao)&&glIsTexture(shared_texture));
    assert(g_engine_perf.static_assets==1&&g_engine_perf.static_instances==1&&g_engine_perf.cache_hits==1);
    // Distinct files/assets with identical embedded pixels share textures, while
    // each Scene retains its own ownership and independently loaded geometry.
    char second_import[256];
    assert(engine_project_import_glb(source,second_import,sizeof(second_import),message,sizeof(message)));
    Scene second_asset={0};int second_handle=0;
    assert(static_mesh_cache_load(second_import,&second_asset,&second_handle));
    assert(second_handle!=shared_handle&&second_asset.meshes[0].texture==shared_texture);
    assert(texture_cache_stats().hits>0);
    static_mesh_cache_release(second_handle);
    // Repeated runtime-model reloads must release old field references.
    g_npc_model_count=1;snprintf(g_npc_model_paths[0],sizeof(g_npc_model_paths[0]),"%s",imported);
    npc_runtime_models_load();size_t npc_refs=texture_cache_stats().references;
    npc_runtime_models_load();assert(texture_cache_stats().references==npc_refs);
    charmodel_free(&g_npc_models[0]);g_npc_model_ok[0]=false;g_npc_model_count=0;
    // A rescan cancels stale decode work. Returned images must fit the UI budget.
    unsigned generation=thumbnail_new_generation();assert(thumbnail_request(generation,9,"missing.glb"));
    generation=thumbnail_new_generation();assert(thumbnail_request(generation,0,imported));
    ThumbnailResult thumbnail={0};bool found=false;Uint64 deadline=SDL_GetTicks64()+10000;
    while(SDL_GetTicks64()<deadline&&!found){
        if(thumbnail_poll(&thumbnail)){assert(thumbnail.generation==generation&&thumbnail.index==0);found=true;}else SDL_Delay(2);
    }
    assert(found&&thumbnail.pixels&&thumbnail.width<=192&&thumbnail.height<=192);stbi_image_free(thumbnail.pixels);thumbnail_shutdown();
    EditorCommandResult result={0};assert(editor_command_prop_set_position(0,9,4,11,&g_placer,&result));
    assert(result.changed&&g_props.list[0].pos.x==9&&g_props.list[0].aabb.min.x>7);
    g_props.list[0].action_count=1;g_props.list[0].actions[0].type=PROP_ACTION_READ_PROMPT;
    snprintf(g_props.list[0].actions[0].prompt_text,sizeof(g_props.list[0].actions[0].prompt_text),"The crossing is safe.");
    g_props.list[0].actions[0].radius=4;
    float before=terrain_h(5,7);sculpt_brush(5,7,8,2,0,0);float raised=terrain_h(5,7);assert(raised>before+1);
    g_placed_lights.count=1;PlacedLight *light=&g_placed_lights.items[0];memset(light,0,sizeof(*light));
    light->active=light->enabled=true;light->uid=5000;light->pos=(Vec3){5,7,9};light->color=(Vec3){1,.6f,.3f};light->radius=12;light->intensity=2;light->flicker_speed=3;
    placed_light_set_defaults(light);
    Terrain terrain={0};SaveNotify note={0};
    actions.terrain=&terrain;actions.forest=&g_forest;actions.world_markers=&g_world_markers;
    actions.world_fx=&g_world_fx;actions.spawns=&g_spawns;actions.npc_spawns=&g_npc_spawns;actions.save_note=&note;
    editor_shell_save_world(&actions);
    struct stat sculpt_file;assert(stat("terrain_sculpt.bin",&sculpt_file)==0);
    props_free(&g_props);memset(&g_props,0,sizeof(g_props));placer_load(&g_props);
    assert(g_props.list[0].meshes[0].vao==shared_vao);
    assert(g_props.count==1&&g_props.list[0].pos.x==9&&g_props.list[0].pos.z==11);
    assert(g_props.list[0].actions[0].type==PROP_ACTION_READ_PROMPT);
    assert(!strcmp(g_props.list[0].actions[0].prompt_text,"The crossing is safe."));
    memset(&g_sculpt,0,sizeof(g_sculpt));g_sculpt_active=false;sculpt_load();assert(fabsf(terrain_h(5,7)-raised)<.0001f);
    placed_lights_load(&g_placed_lights);assert(g_placed_lights.count==1&&g_placed_lights.items[0].uid==5000&&g_placed_lights.items[0].radius==12);
    props_free(&g_props);
    static_mesh_cache_shutdown();assert(!glIsVertexArray(shared_vao)&&!glIsTexture(shared_texture));
    assert(texture_cache_stats().entries==0&&texture_cache_stats().references==0);
    texture_cache_shutdown();
    assert(glGetError()==GL_NO_ERROR);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    puts("PASS studio integration: validated import, search, model placement, transform/collision, Save World, landscape/light/readable round trip, shader compilation, shared-resource lifetime, terrain buffer cleanup, grass paint texture invalidation and async thumbnail cancellation");
    return 0;
}

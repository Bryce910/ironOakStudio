#include "engine/editor_ui.h"
#include "engine/editor_ui_imgui.h"

#include <stdlib.h>
#include <string.h>

#ifndef ENGINE_ENABLE_IMGUI
#define ENGINE_ENABLE_IMGUI 0
#endif

static bool editor_ui_env_requests_imgui(void){
    const char *value=getenv("EDITOR_UI_BACKEND");
    if(!value||!value[0]) return true;
    if(strcmp(value,"hud")==0||strcmp(value,"HUD")==0) return false;
    return strcmp(value,"imgui")==0||strcmp(value,"IMGUI")==0;
}

void editor_ui_init(EditorUIState *state){
    if(!state) return;
    state->imgui_compiled=(ENGINE_ENABLE_IMGUI!=0);
    state->imgui_requested=editor_ui_env_requests_imgui();
    state->backend=(state->imgui_compiled&&state->imgui_requested)
        ?EDITOR_UI_BACKEND_IMGUI
        :EDITOR_UI_BACKEND_HUD;
}

const char *editor_ui_backend_name(const EditorUIState *state){
    if(!state) return "HUD";
    switch(state->backend){
        case EDITOR_UI_BACKEND_IMGUI: return "ImGui";
        case EDITOR_UI_BACKEND_HUD:
        default: return "HUD";
    }
}

bool editor_ui_backend_is_imgui(const EditorUIState *state){
    return state&&state->backend==EDITOR_UI_BACKEND_IMGUI;
}

bool editor_ui_startup(EditorUIState *state,void *window,void *gl_context){
    if(!state) return false;
    if(state->backend==EDITOR_UI_BACKEND_IMGUI){
        if(!editor_ui_imgui_init(window,gl_context)){
            state->backend=EDITOR_UI_BACKEND_HUD;
            return false;
        }
    }
    return true;
}

void editor_ui_shutdown(EditorUIState *state){
    if(!state) return;
    if(state->backend==EDITOR_UI_BACKEND_IMGUI) editor_ui_imgui_shutdown();
}

void editor_ui_begin_frame(const EditorUIState *state){
    if(state&&state->backend==EDITOR_UI_BACKEND_IMGUI) editor_ui_imgui_begin_frame();
}

void editor_ui_begin_passive_frame(const EditorUIState *state,float seconds,int width,int height){
    if(state&&state->backend==EDITOR_UI_BACKEND_IMGUI) editor_ui_imgui_begin_passive_frame(seconds,width,height);
}

void editor_ui_render(const EditorUIState *state){
    if(state&&state->backend==EDITOR_UI_BACKEND_IMGUI) editor_ui_imgui_render();
}

bool editor_ui_process_event(const EditorUIState *state,const SDL_Event *event){
    if(!state||state->backend!=EDITOR_UI_BACKEND_IMGUI) return false;
    return editor_ui_imgui_process_event(event);
}

bool editor_ui_wants_mouse(const EditorUIState *state){
    return state&&state->backend==EDITOR_UI_BACKEND_IMGUI&&editor_ui_imgui_wants_mouse();
}

bool editor_ui_wants_keyboard(const EditorUIState *state){
    return state&&state->backend==EDITOR_UI_BACKEND_IMGUI&&editor_ui_imgui_wants_keyboard();
}

typedef struct{
    const char *label;
    bool active;
}EditorShellItem;

static void shell_chip(const EditorUIRenderAPI *api,void *ctx,float x,float y,float w,float h,
                       const char *label,bool active){
    api->rect(ctx,x,y,w,h,
              active?0.24f:0.07f,
              active?0.34f:0.10f,
              active?0.13f:0.06f,
              active?0.96f:0.74f);
    if(active) api->rect(ctx,x,y,4.0f,h,0.92f,0.76f,0.30f,0.98f);
    api->text(ctx,x+12.0f,y+5.0f,0.84f,label,
              active?0.96f:0.78f,
              active?0.84f:0.82f,
              active?0.38f:0.68f,
              0.98f);
}

static void shell_panel(const EditorUIRenderAPI *api,void *ctx,float x,float y,float w,float h){
    api->rect(ctx,x-10.0f,y-10.0f,w+20.0f,h+20.0f,0.01f,0.02f,0.01f,0.12f);
    api->rect(ctx,x-4.0f,y-4.0f,w+8.0f,h+8.0f,0.03f,0.05f,0.03f,0.60f);
    api->rect(ctx,x,y,w,h,0.07f,0.11f,0.07f,0.95f);
    api->rect(ctx,x+1.0f,y+1.0f,w-2.0f,22.0f,0.17f,0.24f,0.10f,0.22f);
    api->rect(ctx,x+1.0f,y+23.0f,w-2.0f,1.0f,0.70f,0.58f,0.22f,0.22f);
    api->rect(ctx,x+1.0f,y+h-2.0f,w-2.0f,1.0f,0.10f,0.18f,0.08f,0.24f);
    api->rect(ctx,x,y,w,1.0f,0.18f,0.36f,0.12f,0.98f);
    api->rect(ctx,x,y+h-1.0f,w,1.0f,0.18f,0.36f,0.12f,0.98f);
    api->rect(ctx,x,y,1.0f,h,0.18f,0.36f,0.12f,0.98f);
    api->rect(ctx,x+w-1.0f,y,1.0f,h,0.18f,0.36f,0.12f,0.98f);
}

static void shell_section(const EditorUIRenderAPI *api,void *ctx,float x,float *y,float w,
                          const char *title,const EditorShellItem *items,int item_count){
    float row_h=20.0f;
    float header_h=18.0f;
    float block_h=header_h+8.0f+(float)item_count*row_h+8.0f;
    int i=0;
    shell_panel(api,ctx,x,*y,w,block_h);
    api->text(ctx,x+12.0f,*y+7.0f,0.78f,title,0.58f,0.68f,0.42f,0.96f);
    api->rect(ctx,x+12.0f,*y+22.0f,w-24.0f,1.0f,0.25f,0.34f,0.16f,0.32f);
    for(i=0;i<item_count;i++){
        float iy=*y+header_h+8.0f+(float)i*row_h;
        api->rect(ctx,x+8.0f,iy-1.0f,w-16.0f,row_h-2.0f,0.10f,0.16f,0.09f,0.15f);
        if(items[i].active){
            api->rect(ctx,x+8.0f,iy-1.0f,w-16.0f,row_h-2.0f,0.24f,0.34f,0.14f,0.34f);
            api->rect(ctx,x+8.0f,iy-1.0f,4.0f,row_h-2.0f,0.92f,0.76f,0.30f,0.98f);
        }
        api->text(ctx,x+16.0f,iy+3.0f,0.84f,">",
                  items[i].active?0.97f:0.54f,
                  items[i].active?0.83f:0.66f,
                  items[i].active?0.36f:0.40f,
                  items[i].active?1.0f:0.78f);
        api->text(ctx,x+22.0f,iy+3.0f,0.94f,items[i].label,
                  items[i].active?0.96f:0.86f,
                  items[i].active?0.82f:0.80f,
                  items[i].active?0.38f:0.62f,
                  items[i].active?1.0f:0.90f);
    }
    *y+=block_h+10.0f;
}

static void shell_stat(const EditorUIRenderAPI *api,void *ctx,float x,float y,float w,
                       const char *label,const char *value){
    shell_panel(api,ctx,x,y,w,46.0f);
    api->text(ctx,x+12.0f,y+7.0f,0.78f,label,0.58f,0.68f,0.42f,0.94f);
    api->rect(ctx,x+12.0f,y+18.0f,w-24.0f,1.0f,0.25f,0.34f,0.16f,0.38f);
    api->text(ctx,x+12.0f,y+25.0f,1.02f,value,0.94f,0.82f,0.56f,0.99f);
}

static void shell_inspector(const EditorUIRenderAPI *api,void *ctx,float x,float y,float w,float h,
                            const char *label,const char *line_a,const char *line_b,const char *line_c){
    shell_panel(api,ctx,x,y,w,h);
    api->text(ctx,x+12.0f,y+7.0f,0.78f,label,0.58f,0.68f,0.42f,0.94f);
    api->rect(ctx,x+12.0f,y+18.0f,w-24.0f,1.0f,0.25f,0.34f,0.16f,0.38f);
    if(line_a&&line_a[0]) api->text(ctx,x+12.0f,y+28.0f,0.88f,line_a,0.94f,0.82f,0.56f,0.99f);
    if(line_b&&line_b[0]) api->text(ctx,x+12.0f,y+42.0f,0.80f,line_b,0.76f,0.88f,0.96f,0.94f);
    if(line_c&&line_c[0]) api->text(ctx,x+12.0f,y+55.0f,0.78f,line_c,0.60f,0.74f,0.62f,0.92f);
}

static void draw_world_forge_shell_hud(const EditorUIRenderAPI *api,void *ctx,int win_w,int win_h,
                                       const EditorWorldForgeShellState *state){
    const float sidebar_x=8.0f;
    const float sidebar_y=8.0f;
    const float sidebar_w=256.0f;
    const float topbar_x=sidebar_x+256.0f;
    const float topbar_y=8.0f;
    const float topbar_w=(float)win_w-topbar_x-264.0f;
    float y=sidebar_y;
    EditorShellItem paint_items[]={
        {"Terrain Sculpt",state->sculpt_edit},
        {"Forest Brush",state->forest_edit},
        {"Fence Builder",state->forest_edit&&state->forest_brush_kind==4},
        {"Wind Fields",state->wind_edit},
        {"World FX",state->particle_fx_edit},
        {"Shader Lab",state->shader_edit}
    };
    EditorShellItem world_items[]={
        {"Paths",state->path_edit},
        {"Ponds and Lakes",state->wzone_edit&&!state->wf_edit_mode},
        {"Waterfalls",state->wzone_edit&&state->wf_edit_mode},
        {"Rivers",state->river_edit},
        {"Prop Edit",state->prop_edit},
        {"Model Placer",state->prop_place_mode},
        {"Surface Pieces",state->surface_edit},
        {"Markers",state->marker_edit},
        {"Point Lights",state->plight_edit},
        {"Decals",state->decal_edit},
        {"Sound Zones",state->szone_edit},
        {"Interior Paint",state->interior_edit}
    };
    EditorShellItem actor_items[]={
        {"NPC Editor",state->npc_edit_mode},
        {"Spawn Editor",state->spawn_edit_mode}
    };
    EditorShellItem utility_items[]={
        {"Editor Mode",state->editor_mode&&!state->path_edit&&!state->wzone_edit&&!state->river_edit&&!state->prop_edit&&!state->surface_edit&&!state->particle_fx_edit&&!state->marker_edit&&!state->forest_edit&&!state->wind_edit&&!state->sculpt_edit&&!state->npc_edit_mode&&!state->spawn_edit_mode&&!state->shader_edit},
        {"Exit Editor",false},
        {"Save World",false}
    };

    shell_panel(api,ctx,sidebar_x,sidebar_y,sidebar_w,(float)win_h-16.0f);
    api->title(ctx,sidebar_x+24.0f,y+12.0f,1.42f,"WORLD FORGE",0.79f,0.59f,0.16f,0.98f);
    api->text(ctx,sidebar_x+26.0f,y+36.0f,0.72f,"EDITOR SUITE",0.46f,0.58f,0.33f,0.94f);
    api->rect(ctx,sidebar_x+18.0f,y+54.0f,sidebar_w-36.0f,1.0f,0.18f,0.36f,0.12f,0.72f);
    y+=68.0f;
    api->text(ctx,sidebar_x+24.0f,y-10.0f,0.72f,"Pick a workflow, then choose a tool on the left.",0.58f,0.68f,0.42f,0.90f);

    shell_section(api,ctx,sidebar_x+10.0f,&y,sidebar_w-20.0f,"BUILD",paint_items,(int)(sizeof(paint_items)/sizeof(paint_items[0])));
    shell_section(api,ctx,sidebar_x+10.0f,&y,sidebar_w-20.0f,"LAYOUT",world_items,(int)(sizeof(world_items)/sizeof(world_items[0])));
    shell_section(api,ctx,sidebar_x+10.0f,&y,sidebar_w-20.0f,"POPULATE",actor_items,(int)(sizeof(actor_items)/sizeof(actor_items[0])));
    shell_section(api,ctx,sidebar_x+10.0f,&y,sidebar_w-20.0f,"SESSION",utility_items,(int)(sizeof(utility_items)/sizeof(utility_items[0])));

    shell_panel(api,ctx,topbar_x,topbar_y,topbar_w,56.0f);
    api->text(ctx,topbar_x+16.0f,topbar_y+8.0f,0.72f,"WORKFLOW",0.58f,0.68f,0.42f,0.94f);
    shell_chip(api,ctx,topbar_x+12.0f,topbar_y+24.0f,118.0f,20.0f,"Build",state->sculpt_edit||state->forest_edit||state->wind_edit||state->particle_fx_edit||state->shader_edit);
    shell_chip(api,ctx,topbar_x+138.0f,topbar_y+24.0f,120.0f,20.0f,"Layout",state->path_edit||state->wzone_edit||state->river_edit||state->prop_edit||state->prop_place_mode||state->surface_edit||state->marker_edit||state->plight_edit||state->decal_edit||state->szone_edit||state->interior_edit);
    shell_chip(api,ctx,topbar_x+266.0f,topbar_y+24.0f,128.0f,20.0f,"Populate",state->npc_edit_mode||state->spawn_edit_mode);
    shell_chip(api,ctx,topbar_x+402.0f,topbar_y+24.0f,126.0f,20.0f,"Session",state->editor_mode&&!state->path_edit&&!state->wzone_edit&&!state->river_edit&&!state->prop_edit&&!state->prop_place_mode&&!state->surface_edit&&!state->particle_fx_edit&&!state->marker_edit&&!state->forest_edit&&!state->wind_edit&&!state->sculpt_edit&&!state->npc_edit_mode&&!state->spawn_edit_mode&&!state->shader_edit&&!state->interior_edit);

    {
        float sx=(float)win_w-248.0f;
        shell_stat(api,ctx,sx,12.0f,236.0f,"ACTIVE MODE",state->active_mode_text);
        shell_stat(api,ctx,sx,64.0f,236.0f,"EDITOR UI",state->editor_ui_text);
        shell_stat(api,ctx,sx,116.0f,236.0f,"WORLD CONTENT",state->world_content_text);
        shell_stat(api,ctx,sx,168.0f,236.0f,"POPULATION",state->population_text);
        shell_inspector(api,ctx,sx,220.0f,236.0f,82.0f,state->inspect_label,
                        state->inspect_line_a,state->inspect_line_b,state->inspect_line_c);
    }

    {
        float strip_h=40.0f;
        float y0=(float)win_h-strip_h-8.0f;
        shell_panel(api,ctx,sidebar_x+264.0f,y0,(float)win_w-264.0f-264.0f,strip_h);
        api->text(ctx,sidebar_x+282.0f,y0+10.0f,0.90f,
                  "Esc Exit Editor   |   LMB Select or Place   |   RMB Remove or Back",
                  0.83f,0.77f,0.56f,0.92f);
        api->text(ctx,sidebar_x+282.0f,y0+23.0f,0.76f,
                  "Right drag orbit   |   Middle drag pan   |   Scroll zoom   |   Save World from the shell or active tool panel",
                  0.48f,0.60f,0.35f,0.90f);
    }
}

void editor_ui_draw_world_forge_shell(const EditorUIState *ui,
                                      const EditorUIRenderAPI *api,
                                      void *ctx,
                                      int win_w,int win_h,
                                      const EditorWorldForgeShellState *state){
    if(!api||!api->rect||!api->text||!api->title||!state) return;
    switch(ui?ui->backend:EDITOR_UI_BACKEND_HUD){
        case EDITOR_UI_BACKEND_IMGUI:
            editor_ui_imgui_draw_world_forge_shell(state,win_w,win_h);
            break;
        case EDITOR_UI_BACKEND_HUD:
        default:
            draw_world_forge_shell_hud(api,ctx,win_w,win_h,state);
            break;
    }
}

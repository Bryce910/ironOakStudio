// Production submission predicate with actual public ImGui data, lists,
// callbacks and textures. Core ImGui objects only; no SDL or GL context.
#include "engine/editor_ui_imgui_submission.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

namespace {
unsigned callbacks=0;
void callback(const ImDrawList *,const ImDrawCmd *){callbacks++;}
bool skip(const ImDrawData &draw){return world_imgui_can_skip_empty_backend(true,true,&draw);}
}

int main(){
    ImGuiContext *context=ImGui::CreateContext();assert(context);
    ImGui::GetIO().IniFilename=nullptr;
    {
        ImDrawData draw;
        assert(!skip(draw));draw.Valid=true;
        assert(skip(draw)&&world_imgui_pending_texture_work(&draw)==0);
        assert(!world_imgui_can_skip_empty_backend(false,true,&draw));
        assert(!world_imgui_can_skip_empty_backend(true,false,&draw));
        assert(!world_imgui_can_skip_empty_backend(true,true,nullptr));

        ImTextureData texture,other;
        texture.SetStatus(ImTextureStatus_OK);other.SetStatus(ImTextureStatus_OK);
        ImVector<ImTextureData*> textures;textures.push_back(&texture);textures.push_back(&other);
        draw.Textures=&textures;
        assert(skip(draw)&&world_imgui_pending_texture_work(&draw)==0);
        for(auto status:{ImTextureStatus_WantCreate,ImTextureStatus_WantUpdates,
                         ImTextureStatus_WantDestroy,ImTextureStatus_Destroyed}){
            texture.SetStatus(status);
            assert(texture.Status==status&&!skip(draw)&&world_imgui_pending_texture_work(&draw)==1);
            assert(texture.Status==status&&textures.Size==2); // Predicate never services/mutates requests.
        }
        other.SetStatus(ImTextureStatus_WantDestroy);
        assert(!skip(draw)&&world_imgui_pending_texture_work(&draw)==2);
        texture.SetStatus(ImTextureStatus_OK);other.SetStatus(ImTextureStatus_OK);
        textures.push_back(nullptr);assert(!skip(draw)&&world_imgui_pending_texture_work(&draw)==1);textures.pop_back();
        assert(skip(draw));

        ImDrawList callback_list(ImGui::GetDrawListSharedData());
        callback_list.AddDrawCmd();callback_list.AddCallback(callback,nullptr);
        draw.AddDrawList(&callback_list);
        assert(draw.CmdListsCount==1&&draw.CmdLists.Size==1&&draw.TotalVtxCount==0&&draw.TotalIdxCount==0);
        assert(!skip(draw)&&callbacks==0);
        bool found_callback=false;
        for(const auto &command:callback_list.CmdBuffer)if(command.UserCallback){
            found_callback=true;command.UserCallback(&callback_list,&command);
        }
        assert(found_callback&&callbacks==1); // There is real work despite zero geometry.
        draw.CmdListsCount=0;assert(!skip(draw)); // Inconsistent count cannot hide a callback list.
        draw.Clear();draw.Valid=true;

        ImDrawList geometry(ImGui::GetDrawListSharedData());
        geometry.AddDrawCmd();geometry.PushClipRect(ImVec2(0,0),ImVec2(640,480));
        geometry.AddRectFilled(ImVec2(10,10),ImVec2(80,60),IM_COL32_WHITE);
        geometry.PopClipRect();draw.AddDrawList(&geometry);
        assert(draw.CmdListsCount==1&&draw.TotalVtxCount>0&&draw.TotalIdxCount>0&&!skip(draw));
        const int vertices=draw.TotalVtxCount,indices=draw.TotalIdxCount;
        assert(!world_imgui_can_skip_empty_backend(true,false,&draw));
        assert(draw.TotalVtxCount==vertices&&draw.TotalIdxCount==indices&&draw.CmdLists.Size==1);
        draw.Clear();draw.Valid=true;

        draw.CmdListsCount=1;assert(!skip(draw));draw.CmdListsCount=0;
        draw.TotalVtxCount=1;assert(!skip(draw));draw.TotalVtxCount=0;
        draw.TotalIdxCount=1;assert(!skip(draw));draw.TotalIdxCount=-1;assert(!skip(draw));draw.TotalIdxCount=0;
        assert(skip(draw));draw.Valid=false;assert(!skip(draw));
    }
    ImGui::DestroyContext(context);
    std::puts("PASS production ImGui submission: actual empty/nonempty/callback-only lists; reference/visible/invalid guards; clean/create/update/destroy/null texture requests; zero decision mutation; no SDL/GL context");
}

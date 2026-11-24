#include "engine/world_runtime_demo.h"
#include "engine/world_studio.h"
#include "engine/world_editor_session.h"
#include "third_party/imgui/imgui.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>
#include <unistd.h>
// This harness covers the Studio/coordinator lifecycle without a model or GL
// host. Native integration validates the real visual-admission implementation.
bool world_live_visuals_validate(const world_studio::Document &,const world_runtime::Document &,const world_runtime_demo::Seed &,const std::vector<world_runtime::Entity> &,std::string &){return true;}
struct Camera {};
struct ComponentsStudio {
    world_studio::Document live,draft;world_physics::Document physics_document;world_gameplay::Document gameplay_document;world_living::World living_world;world_living::Definition living_design;
    std::string project_directory,message;bool physics_modified=false,gameplay_dirty=false,gameplay_baseline_valid=false,gameplay_editing=false,living_ready=false,living_modified=false,playing=false,dirty=false,target=true;int physics_selection=-1,tab=0,requested_tab=24;double living_clock_poll=0,living_save_clock=0,x=0,y=24,z=0;
    bool production_can_save()const{return !playing;}
    bool production_playing()const{return playing;}
    bool production_freeze()const{return false;}
    static double production_now(){return 1000;}
    void physics_reseed_ids(){}void physics_changed(){physics_modified=true;}
    bool gameplay_text(const char *,std::string &,unsigned){return false;}
    #include "engine/world_studio_components_view.inc"
};
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<" | "<<studio.message<<'\n';return 1;}}while(0)
int main(){
    ComponentsStudio studio;char folder[]="/tmp/valwood-components-studio-XXXXXX";CHECK(mkdtemp(folder));studio.project_directory=folder;
    world_entity::Node authored;authored.id=5;authored.name="Existing user marker";authored.local={1,2,3,90,2};authored.visible=false;studio.live.entities.nodes={authored};studio.live.entities.next_id=6;std::string error;std::vector<world_runtime::Entity> entities;CHECK(studio.components_scene_entities(studio.live,entities,error));CHECK(entities.size()==1&&entities[0].id==5&&!entities[0].active&&entities[0].scale==2);
    studio.components_init();CHECK(!studio.components_manifest_loaded&&!studio.components_manifest_invalid);CHECK(studio.components_start(1000,2000,{-10000,120,8000}));CHECK(studio.components_ready&&studio.components_generation==1&&studio.living_ready&&studio.components_demo.visuals.size()==8);CHECK(studio.live.entities.nodes.size()==1&&studio.live.entities.nodes[0].id==5);CHECK(studio.components_frame->entity_count==12&&studio.components_frame->motion_count==6);
    CHECK(studio.components_interact({957.5,24,1932}));studio.components_service(.016,studio.components_wall(),{957.5,24,1932},true);CHECK(studio.components_runtime->current_line());CHECK(studio.components_runtime->choose(0));studio.components_service(.016,studio.components_wall(),{957.5,24,1932},true);world_gameplay::Command command;bool training=false;while(studio.components_poll_request(command))training|=command.kind==world_gameplay::CommandKind::StartTraining;CHECK(training);CHECK(!studio.components_poll_request(command));
    CHECK(studio.components_play_begin());const auto generation=studio.components_generation;world_runtime::Entity original;CHECK(studio.components_runtime->entity(world_runtime_demo::jungle_actor_first,original));auto moved=original.pose;moved.position.x+=20;CHECK(studio.components_runtime->set_scene_pose(original.id,moved));CHECK(studio.components_play_stop(world_editor::StopChoice::DiscardPlayChanges));world_runtime::Entity restored;CHECK(studio.components_runtime->entity(original.id,restored)&&std::abs(restored.pose.position.x-original.pose.position.x)<1e-8&&studio.components_generation==generation&&studio.components_rollback_serial==1);
    CHECK(studio.components_scene_applied()&&studio.components_generation==generation);studio.live.entities.nodes[0].local.x=5;CHECK(studio.components_scene_applied()&&studio.components_generation==generation+1);CHECK(studio.components_runtime->entity(original.id,restored)&&std::abs(restored.pose.position.x-original.pose.position.x)<1e-8);
    CHECK(studio.components_play_begin());CHECK(studio.components_runtime->entity(original.id,original));const auto living_tick=studio.living_world.state().tick;CHECK(studio.components_runtime->set_scene_pose(original.id,moved));studio.living_design.actors[0].name="Authored role label kept";CHECK(studio.living_world.observe_clock(studio.components_wall()+60,error));studio.living_world.process(1);CHECK(studio.living_world.state().tick>living_tick);CHECK(studio.components_play_stop(world_editor::StopChoice::KeepAuthoringChanges));CHECK(studio.components_runtime->entity(original.id,restored)&&std::abs(restored.pose.position.x-original.pose.position.x)<1e-8&&studio.living_world.state().tick==living_tick&&studio.components_rollback_serial==2);CHECK(studio.living_design.actors[0].name=="Authored role label kept");
    // The host owns this one living World. A foreign reset after Play begins
    // restores the saved logical definition; a reset before Play blocks entry.
    const auto saved_living=studio.living_world;CHECK(studio.components_play_begin());
    auto foreign=saved_living.definition();foreign.actors.resize(1);foreign.actors[0].id=123;
    CHECK(studio.living_world.configure(foreign,studio.components_wall(),error));
    CHECK(studio.components_can_stop(world_editor::StopChoice::DiscardPlayChanges));
    CHECK(studio.components_play_stop(world_editor::StopChoice::DiscardPlayChanges));
    CHECK(studio.living_world.definition().actors.size()==6&&studio.components_frame->motion_count==6&&studio.components_rollback_serial==3);
    CHECK(studio.living_world.configure(foreign,studio.components_wall(),error));
    CHECK(!studio.components_play_begin()&&!studio.components_play_snapshot_ready);
    studio.living_world=saved_living;CHECK(studio.components_play_begin());
    CHECK(studio.components_play_stop(world_editor::StopChoice::DiscardPlayChanges));
    studio.dirty=true;CHECK(!studio.components_configure_current());studio.dirty=false;studio.playing=true;CHECK(!studio.components_configure_current());studio.playing=false;CHECK(studio.components_configure_current());
    studio.components_document.physics[0].entity=5;
    CHECK(studio.components_active_document.physics[0].entity==world_runtime_demo::castle_crate_id);
    auto pending_removed=studio.live;pending_removed.entities.nodes.clear();CHECK(studio.components_scene_prevalidate(pending_removed,error));
    CHECK(studio.components_configure_current());CHECK(studio.components_active_document.physics[0].entity==5);
    auto invalid_install=studio.components_demo.configuration;invalid_install.components.living[0].actor=UINT32_MAX;
    CHECK(!studio.components_install(invalid_install)&&studio.components_active_document.physics[0].entity==5&&studio.components_ready);
    CHECK(studio.components_scene_prevalidate(studio.live,error));auto removed=studio.live;removed.entities.nodes.clear();CHECK(!studio.components_scene_prevalidate(removed,error));auto scaled=studio.live;scaled.entities.nodes[0].local.scale=3;CHECK(!studio.components_scene_prevalidate(scaled,error));CHECK(studio.components_runtime->entity(5,restored));
    CHECK(studio.components_export_demo_assets());CHECK(!studio.components_export_demo_assets());CHECK(std::filesystem::exists(std::filesystem::path(folder)/"world.components"));
    ComponentsStudio reopened;reopened.project_directory=folder;reopened.components_init();CHECK(reopened.components_manifest_loaded);CHECK(reopened.components_start(1000,2000,{-10000,120,8000}));CHECK(reopened.components_ready&&reopened.components_frame->motion_count==6);
    // A saved custom logical definition initializes directly; the fallback's
    // IDs cannot replace the authored set merely because there is no player save.
    auto custom_definition=studio.living_world.definition();for(auto &actor:custom_definition.actors)actor.id+=123;world_living::World custom;CHECK(custom.configure(custom_definition,0,error));CHECK(custom.save((std::filesystem::path(folder)/"runtime-demo.living").string(),error));auto custom_manifest=studio.components_document;for(auto &binding:custom_manifest.living)binding.actor+=123;CHECK(world_runtime::save((std::filesystem::path(folder)/"world.components").c_str(),custom_manifest,error));ComponentsStudio custom_reopened;custom_reopened.project_directory=folder;custom_reopened.components_init();CHECK(custom_reopened.components_start(1000,2000,{-10000,120,8000}));CHECK(custom_reopened.components_frame->motion[0].actor==920124&&custom_reopened.components_demo.visuals[0].living_actor==920124);
    auto *context=ImGui::CreateContext();auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={1400,1000};io.DeltaTime=1.f/60;unsigned char *pixels=nullptr;int w=0,h=0;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    for(unsigned i=0;i<5;i++){ImGui::NewFrame();ImGui::SetNextWindowSize({700,900});ImGui::Begin("Live component harness");CHECK(ImGui::BeginTabBar("Tools"));studio.components_panel({});ImGui::EndTabBar();ImGui::End();studio.components_dialogue();ImGui::Render();CHECK(studio.tab==24);}
    ImGui::DestroyContext(context);std::filesystem::remove_all(folder);std::cout<<"world_components_studio: "<<checks<<" actual ImGui/startup/dialogue/training/Play/manifest contracts\n";
}

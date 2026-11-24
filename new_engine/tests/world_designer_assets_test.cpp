#include "engine/world_designer_assets.h"
#include "engine/world_designer_viewport.h"
#include "engine/world_studio.h"
#include "third_party/imgui/imgui.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

using namespace world_designer_assets;
namespace {
std::vector<WorldSceneryAlias> registry;
std::uint64_t registry_revision=1,registry_reads=0;
WorldSceneryAlias asset(const char *name,const char *stem=nullptr,unsigned collision=1,unsigned lods=2){
    WorldSceneryAlias a;a.name=name;a.stem=stem?stem:name;a.lods=lods;a.collision=collision;a.distance=240;a.split=60;return a;
}
struct Camera {};
struct Point {double x,y,z;};
struct Browser {
    struct Draft {std::vector<world_studio::Placement> placements;} draft;
    bool target=false,open=true,locked=false,pick_ground=true,blocked=false,apply=false;
    double x=1,y=2,z=3;float yaw=20,scale=1;
    int alias=0,requested_tab=-1;unsigned checkpoints=0,changes=0,chosen=0,prefs=0,picks=0;
    std::string message,inspected;WorldScenery *scene_ptr=nullptr;
    bool production_can_save()const{return !locked;}
    bool production_playing()const{return false;}
    bool production_freeze()const{return false;}
    bool designer_blocks_viewport()const{return blocked;}
    void checkpoint(){++checkpoints;}
    void changed(bool geometry){assert(geometry);++changes;}
    void choose(unsigned key,bool append,bool expand){assert(!append&&!expand);chosen=key;}
    void inspect_asset(const std::string &name){inspected=name;}
    void designer_preferences_changed(){++prefs;}
    world_designer_viewport::Rect designer_viewport(float,float)const{return {200,100,700,400};}
    void pick(const Camera &,float mx,float my){++picks;target=pick_ground;x=mx;y=2;z=my;}
    bool project_point(const Camera &,Point p,ImVec2 &out)const{out={float(200+p.x*.1),float(300-p.y*.1)};return true;}
    #include "engine/world_studio_designer_assets.inc"
};
void begin_frame(ImVec2 mouse={0,0},bool down=false){
    auto &io=ImGui::GetIO();io.AddMousePosEvent(mouse.x,mouse.y);io.AddMouseButtonEvent(0,down);ImGui::NewFrame();
    ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({1200,215});
    ImGui::Begin("Content",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoSavedSettings);
}
void end_frame(){ImGui::End();ImGui::Render();}
void payload(const char *name){
    assert(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceExtern|ImGuiDragDropFlags_SourceNoPreviewTooltip));
    AliasPayload encoded;assert(encode_payload(name,encoded));ImGui::SetDragDropPayload(payload_type,&encoded,sizeof encoded);ImGui::EndDragDropSource();
}
}
std::size_t world_scenery_alias_count(){return registry.size();}
std::uint64_t world_scenery_alias_revision() noexcept{return registry_revision;}
WorldSceneryAlias world_scenery_alias(std::size_t index){++registry_reads;return index<registry.size()?registry[index]:WorldSceneryAlias{};}
bool world_scenery_alias_bounds(const WorldScenery *,const char *,float[3],float[3]){return false;}

static void helpers(){
    assert(humanize("bep_nature_oak")=="Bep Nature Oak");
    assert(humanize("rock2")=="Rock 2");assert(humanize("_house--A_12 ")=="House A 12");
    auto a=asset("castle_weapon_rack");assert(classify(a)==Category::Props);
    a=asset("purchased_torch_wall");assert(classify(a)==Category::Props);
    a=asset("castle_training_dummy");assert(classify(a)==Category::Props);
    a=asset("purchased_castle");assert(classify(a)==Category::Buildings);
    a=asset("bep_structure_storehouse");assert(classify(a)==Category::Buildings);
    a=asset("village_horseman");assert(classify(a)==Category::Characters);
    a=asset("starting_celandine");assert(classify(a)==Category::Nature);
    a=asset("my_custom_asset");a.tree=true;assert(classify(a)==Category::Nature);
    a=asset("purchased_house_b","purchased_village_house_b",4,2);const auto r=record(7,a);
    assert(r.index==7&&r.alias==a.name&&r.stem==a.stem&&r.lods==2&&r.collision==4&&r.distance==240);
    assert(matches(r,"HOUSE purchased"));assert(matches(r,"\tVILLAGE\n house_b "));
    assert(matches(r,"village-house"));assert(!matches(r,"house jungle"));assert(matches(r," \t"));
    const auto sig=signature(a);a.collision=0;assert(signature(a)!=sig);a.collision=4;a.distance=241;assert(signature(a)!=sig);
    std::vector<std::string> favorite;assert(toggle_favorite(favorite,"pine"));assert(contains(favorite,"pine"));
    assert(!contains(favorite,"PINE"));assert(toggle_favorite(favorite,"pine")&&favorite.empty());
    for(unsigned i=0;i<favorite_limit;++i)assert(toggle_favorite(favorite,"model_"+std::to_string(i)));
    assert(!toggle_favorite(favorite,"extra"));assert(!toggle_favorite(favorite,"../evil"));
    assert(toggle_favorite(favorite,"model_0")&&favorite.size()==favorite_limit-1);
    std::vector<std::string> recent;for(unsigned i=0;i<50;++i)remember(recent,"model_"+std::to_string(i));
    assert(recent.size()==recent_limit&&recent.front()=="model_49");remember(recent,recent.back());assert(recent.front()=="model_34");
    auto prefs=std::vector<std::string>{"pine","bad/path","pine","house","","fern"};normalize_preferences(prefs,2);
    assert((prefs==std::vector<std::string>{"pine","house"}));normalize_preferences(prefs,0);assert(prefs.empty());
    AliasPayload payload;assert(encode_payload("bep_nature_oak",payload));std::string_view decoded;
    assert(decode_payload(&payload,sizeof payload,decoded)&&decoded=="bep_nature_oak");
    assert(!decode_payload(&payload,sizeof payload-1,decoded));assert(!decode_payload(nullptr,sizeof payload,decoded));
    std::memset(payload.alias,'x',sizeof payload.alias);assert(!decode_payload(&payload,sizeof payload,decoded));
    assert(encode_payload(std::string(95,'x'),payload));assert(!encode_payload(std::string(96,'x'),payload));
    assert(!encode_payload("bad/name",payload));assert(!valid_alias(std::string("x\0y",3)));
    float yaw=0,scale=0;a=asset("pine");a.max_scale=2;
    assert(placement_parameters(a,50400,-28400,740,4,yaw,scale)&&yaw==20&&scale==2);
    assert(!placement_parameters(a,50400.1,0,0,1,yaw,scale));assert(!placement_parameters(a,0,28400.1,0,1,yaw,scale));
    assert(!placement_parameters(a,0,0,std::numeric_limits<float>::quiet_NaN(),1,yaw,scale));
    assert(!placement_parameters(a,std::numeric_limits<double>::infinity(),0,0,1,yaw,scale));
}
static void browser(){
    registry={asset("pine"),asset("purchased_house_a"),asset("castle_weapon_rack"),asset("village_horseman")};registry[0].tree=true;
    ++registry_revision;Browser b;b.designer_asset_refresh();assert(b.designer_asset_records.size()==4);
    const auto *cache=b.designer_asset_records.data();b.designer_asset_refresh();assert(cache==b.designer_asset_records.data());
    const auto reads=registry_reads;for(unsigned frame=0;frame<100;++frame){b.designer_asset_refresh();assert(b.designer_asset_index("village_horseman")==3);}
    assert(registry_reads==reads&&b.designer_asset_registry.rebuilds()==1);
    b.target=true;assert(b.designer_place_asset(0));assert(b.checkpoints==1&&b.changes==1&&b.chosen==8192&&b.draft.placements.size()==1&&b.apply);
    const auto &p=b.draft.placements.back();assert(p.alias=="pine"&&p.x==1&&p.z==3&&p.yaw==20&&p.scale==1&&p.burial==0&&p.elevation==0&&p.offset_y==0);
    assert(b.designer_asset_recent.front()=="pine"&&b.requested_tab==2);
    Browser manual;manual.target=true;manual.designer_object_preview=false;
    assert(manual.designer_place_asset(0)&&manual.draft.placements.size()==1&&!manual.apply);
    b.apply=false;b.designer_object_preview=false;b.designer_preview_object_edit();assert(!b.apply);b.designer_object_preview=true;
    b.locked=true;assert(!b.designer_place_asset(1));assert(b.checkpoints==1&&!b.apply);b.locked=false;
    b.target=false;assert(!b.designer_place_asset(1)&&b.checkpoints==1);b.target=true;
    assert(!b.designer_place_asset(99)&&b.checkpoints==1);
    b.draft.placements.resize(world_studio::placement_limit);assert(!b.designer_place_asset(1)&&b.checkpoints==1);b.draft.placements.resize(1);
    b.designer_asset_selected="purchased_house_a";b.designer_asset_pending="purchased_house_a";registry.resize(1);++registry_revision;b.designer_asset_refresh();
    assert(b.designer_asset_selected.empty()&&!b.designer_asset_placing());
    registry={asset("pine"),asset("purchased_house_a"),asset("castle_weapon_rack"),asset("village_horseman")};registry[0].tree=true;++registry_revision;
    begin_frame();b.designer_assets_panel();end_frame();assert(b.designer_asset_filtered.size()==4);
    std::strcpy(b.designer_asset_search,"HOUSE");b.designer_asset_refilter=true;
    begin_frame();b.designer_assets_panel();end_frame();assert(b.designer_asset_filtered.size()==1&&b.designer_asset_records[b.designer_asset_filtered[0]].alias=="purchased_house_a");
    b.designer_asset_search[0]=0;b.designer_asset_category=int(Category::Nature);b.designer_asset_refilter=true;
    begin_frame();b.designer_assets_panel();end_frame();assert(b.designer_asset_filtered.size()==1&&b.designer_asset_filtered[0]==0);
    // A transaction may replace every backing string and all metadata without
    // changing the count. Names, not stale numeric indices, preserve selection.
    b.designer_asset_selected="pine";b.designer_asset_pending="pine";
    b.designer_asset_favorites={"pine","temporarily_unavailable"};b.designer_asset_recent={"pine"};
    const auto rebuilds=b.designer_asset_registry.rebuilds();
    const std::string_view borrowed=b.designer_asset_records[0].alias;
    registry={asset("purchased_house_a"),asset("castle_weapon_rack"),asset("village_horseman"),asset("pine","reimported_pine",3,3)};
    registry[3].tree=true;registry[3].distance=520;registry[3].split=105;++registry_revision;
    assert(b.designer_asset_index(borrowed)==3&&b.alias==3&&b.designer_asset_pending=="pine");
    assert(b.designer_asset_registry.rebuilds()==rebuilds+1&&b.designer_asset_refilter);
    const auto &updated=b.designer_asset_records[3];assert(updated.stem=="reimported_pine"&&updated.lods==3&&updated.collision==3&&updated.distance==520&&updated.split==105);
    assert((b.designer_asset_favorites==std::vector<std::string>{"pine","temporarily_unavailable"}));assert(b.designer_asset_recent.front()=="pine");
    registry[3]=asset("replacement_rock");++registry_revision;
    assert(b.designer_asset_index(b.designer_asset_selected)<0&&b.designer_asset_selected.empty()&&b.designer_asset_pending.empty());
    assert(b.designer_asset_index("replacement_rock")==3);
    registry={asset("pine"),asset("purchased_house_a"),asset("castle_weapon_rack"),asset("village_horseman")};registry[0].tree=true;++registry_revision;b.designer_asset_refresh();
    b.designer_asset_category=0;b.designer_asset_pending="pine";b.designer_asset_arm_frame=ImGui::GetFrameCount();
    const Camera camera;const auto checkpoints=b.checkpoints;
    begin_frame({100,300},true);b.designer_asset_drop(camera);end_frame();assert(b.checkpoints==checkpoints&&b.designer_asset_placing());
    begin_frame({400,300},false);b.designer_asset_drop(camera);end_frame();assert(b.checkpoints==checkpoints);
    begin_frame({400,300},true);b.designer_asset_drop(camera);end_frame();assert(b.checkpoints==checkpoints+1&&!b.designer_asset_placing());
    assert(b.draft.placements.back().x==400&&b.draft.placements.back().z==300);
    b.designer_asset_pending="pine";b.designer_asset_arm_frame=-1;b.pick_ground=false;b.target=true;
    begin_frame({400,300},false);b.designer_asset_drop(camera);end_frame();assert(!b.target);
    begin_frame({400,300},true);b.designer_asset_drop(camera);end_frame();assert(b.checkpoints==checkpoints+1);b.designer_asset_cancel();
    b.designer_asset_pending="pine";b.blocked=true;
    begin_frame({400,300},false);b.designer_asset_drop(camera);end_frame();
    begin_frame({400,300},true);b.designer_asset_drop(camera);end_frame();assert(b.checkpoints==checkpoints+1);b.blocked=false;b.designer_asset_cancel();
    // Losing application focus and Escape cancel armed placement immediately.
    // Modal ownership and text entry never commit a placement behind controls.
    begin_frame({400,300},false);end_frame();b.designer_asset_pending="pine";
    ImGui::GetIO().AddFocusEvent(false);begin_frame();b.designer_asset_drop(camera);end_frame();assert(b.designer_asset_pending.empty());
    ImGui::GetIO().AddFocusEvent(true);begin_frame();end_frame();b.designer_asset_pending="pine";b.blocked=true;
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,true);begin_frame();b.designer_asset_drop(camera);end_frame();assert(b.designer_asset_pending.empty());b.blocked=false;
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,false);begin_frame();end_frame();
    char focused[32]="search";
    begin_frame();ImGui::SetKeyboardFocusHere();ImGui::InputText("Focused search",focused,sizeof focused);end_frame();
    begin_frame();ImGui::InputText("Focused search",focused,sizeof focused);end_frame();
    b.designer_asset_pending="pine";b.pick_ground=true;
    begin_frame({400,300},true);assert(ImGui::GetIO().WantTextInput);b.designer_asset_drop(camera);end_frame();assert(b.checkpoints==checkpoints+1&&!b.designer_asset_pending.empty());
    b.designer_asset_cancel();begin_frame({400,300},false);end_frame();
    // Real public ImGui payload persists through the release frame. A viewport
    // drop is a single undo transaction; a drop on an editor panel does nothing.
    b.pick_ground=true;
    begin_frame({500,350},true);payload("purchased_house_a");assert(b.designer_asset_placing());b.designer_asset_drop(camera);end_frame();
    assert(b.checkpoints==checkpoints+1);
    begin_frame({500,350},false);b.designer_asset_drop(camera);end_frame();
    assert(b.checkpoints==checkpoints+2&&b.draft.placements.back().alias=="purchased_house_a"&&b.apply);
    assert(b.draft.placements.back().x==500&&b.draft.placements.back().z==350);
    begin_frame();b.designer_asset_drop(camera);end_frame();
    begin_frame({100,350},true);payload("pine");b.designer_asset_drop(camera);end_frame();
    begin_frame({100,350},false);b.designer_asset_drop(camera);end_frame();assert(b.checkpoints==checkpoints+2);
    begin_frame();end_frame();
    // A mid-drag registry replacement cannot turn an obsolete alias index into
    // another model's placement, even if the registry count is unchanged.
    begin_frame({500,350},true);payload("pine");b.designer_asset_drop(camera);end_frame();
    registry[0]=asset("new_pine_replacement");++registry_revision;
    begin_frame({500,350},false);b.designer_asset_drop(camera);end_frame();assert(b.checkpoints==checkpoints+2);
    registry[0]=asset("pine");registry[0].tree=true;++registry_revision;begin_frame();end_frame();
    // Invalid aliases are re-resolved at release rather than trusting an index
    // that could refer to a different model after a registry transaction.
    begin_frame({500,350},true);payload("removed_model");b.designer_asset_drop(camera);end_frame();
    begin_frame({500,350},false);b.designer_asset_drop(camera);end_frame();assert(b.checkpoints==checkpoints+2);
    begin_frame();end_frame();
    // The browser virtualizes rows: 7,000 real registry entries should submit
    // geometry for only the visible cards, never a card per library entry.
    std::vector<std::string> names;names.reserve(7000);registry.clear();registry.reserve(7000);
    for(unsigned i=0;i<7000;++i)names.push_back("purchased_house_"+std::to_string(i));
    for(const auto &name:names)registry.push_back(asset(name.c_str()));
    ++registry_revision;b.designer_asset_refilter=true;
    for(unsigned frame=0;frame<4;++frame){begin_frame();b.designer_assets_panel();end_frame();assert(b.designer_asset_filtered.size()==7000);assert(ImGui::GetDrawData()->TotalVtxCount<20000);}
    const auto warm_reads=registry_reads;const auto warm_rebuilds=b.designer_asset_registry.rebuilds();
    for(unsigned frame=0;frame<100;++frame){begin_frame();b.designer_assets_panel();end_frame();assert(ImGui::GetDrawData()->TotalVtxCount<20000);}
    assert(registry_reads==warm_reads&&b.designer_asset_registry.rebuilds()==warm_rebuilds);
    b.designer_asset_grid=false;for(unsigned frame=0;frame<4;++frame){begin_frame();b.designer_assets_panel();end_frame();assert(ImGui::GetDrawData()->TotalVtxCount<20000);}
    registry.clear();
}
int main(){
    helpers();auto *context=ImGui::CreateContext();auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1200,800};io.DeltaTime=1.f/60;
    unsigned char *pixels=nullptr;int w=0,h=0;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);assert(pixels&&w&&h);
    browser();ImGui::DestroyContext(context);std::puts("designer assets: metadata, filters, safe payloads, bounded preferences, placement transactions, viewport cancellation, and 7,000-entry clipped UI passed");
    return 0;
}

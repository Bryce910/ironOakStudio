// Owned player items and a single modal UI for both current playable routes.
using InvItem=world_player_inventory::Item;
using InvKind=world_player_inventory::Kind;
const bool inventory_enabled=arena_enabled||first_journey_enabled;
int inventory_return_screen=1,inventory_filter=0;
InvItem inventory_selected=InvItem::Sword;
bool inventory_focus=true,inventory_controls_ready=true;
world_arena_progress::NeutralGate inventory_input_gate;
std::string inventory_message;
unsigned inventory_transactions=0,inventory_save_failures=0,inventory_opened=0,inventory_loot_pickups=0;
std::array<ImVec4,unsigned(InvItem::Count)> inventory_item_rects={};
ImVec4 inventory_action_rect={},inventory_close_rect={};
auto inventory_earned=[&]()->std::uint64_t{return world_player_profile::earned(player_profile);};
auto inventory_item_name=[&](InvItem item){return item==InvItem::Sword&&(player_inventory.receipts&world_player_inventory::battlefield_receipt)?"Stolen longsword":world_player_inventory::definition(item).name;};
auto inventory_apply_weapon=[&](){
    equipment_cancel_pending();
    if(player_inventory.weapon==InvItem::None)return;
    auto weapon=world_player_inventory::weapon_type(player_inventory.weapon);
    if(arena_enabled){arena_game.set_weapon(weapon);arena_weapon_draft=int(weapon);arena_target_lock.clear();arena_game.cancel_draw();arena_mouse_draw=arena_mouse_aim=false;arena_bow_armed=false;}
    else story_weapon=weapon;
    fight.weapon=weapon;fight.set_guard(false);fight.clear_buffer();sword_stick.reset();weapon_sheathed=false;
};
// The durable ledger retains belongings across checkpoint rewinds. Project
// battlefield provenance from this journey's actual disarm, so a previous
// playthrough cannot give a fresh unarmed arrival a false sword receipt.
auto inventory_story_snapshot=[&](bool loan,bool supplies,bool rewarded){
    auto view=world_player_profile::story_snapshot(player_profile,loan,supplies,rewarded);
    if(battlefield_has_sword()&&(player_profile.inventory.receipts&world_player_inventory::battlefield_receipt))view.receipts|=world_player_inventory::battlefield_receipt;
    return view;
};
auto inventory_profile_view=[&](){
    if(arena_enabled)return player_profile.inventory;
    const auto &j=first_journey_state.stable_save();
    auto view=inventory_story_snapshot(j.weapon_unlocked,j.supplies_recovered,j.reward_granted);
    view.spent=player_profile.inventory.spent;return view;
};
if(first_journey_enabled&&!player_profile.story_imported){
    world_opening_prologue::Save old;std::string error;std::ifstream file("saves/opening.progress");
    auto next=player_profile;world_player_inventory::State legacy;std::uint64_t coins=0;
    if(file){if(!world_opening_prologue::parse_save(file,opening_state.config(),old,error)){std::fprintf(stderr,"[Player] Existing story save: %s\n",error.c_str());return 1;}
        if(old.inventory_present)legacy=old.inventory;
        else if(old.first_journey_present)world_player_inventory::reconcile(legacy,old.first_journey.weapon_unlocked,old.first_journey.supplies_recovered,old.first_journey.reward_granted);
        if(old.first_journey_present)coins=old.first_journey.coins;
    }
    if(!world_player_profile::import(next,legacy,coins,nullptr,error)||!player_profile_commit(next,error)){std::fprintf(stderr,"[Player] Story migration: %s\n",error.c_str());return 1;}
}
player_inventory=inventory_profile_view();
auto inventory_sync_story=[&](){
    if(!first_journey_enabled)return true;
    const auto &j=first_journey_state.stable_save();auto next=player_profile;
    if(!world_player_profile::reconcile(next,j.weapon_unlocked,j.supplies_recovered,j.reward_granted,inventory_message))return false;
    if((next.inventory.counts!=player_profile.inventory.counts||next.inventory.armor!=player_profile.inventory.armor||next.inventory.weapon!=player_profile.inventory.weapon||next.inventory.receipts!=player_profile.inventory.receipts||next.story_coins!=player_profile.story_coins)&&!player_profile_commit(next,inventory_message))return false;
    player_inventory=inventory_profile_view();return true;
};
inventory_attach_save=[&](world_opening_prologue::Save &checkpoint){
    if(!first_journey_enabled||!checkpoint.first_journey_present)return;
    const auto &j=checkpoint.first_journey;
    checkpoint.inventory_present=true;checkpoint.inventory=inventory_story_snapshot(j.weapon_unlocked,j.supplies_recovered,j.reward_granted);
};
inventory_restore_save=[&](const world_opening_prologue::Save &checkpoint,std::string &error){
    inventory_pending_restore=false;
    if(checkpoint.inventory_present&&!world_player_inventory::valid(checkpoint.inventory)){error="Invalid checkpoint inventory.";return false;}
    if(!checkpoint.battlefield_progress.empty()){
        if(!battlefield_checkpoint_validate_fn){error="The battlefield checkpoint validator is unavailable. Existing progress was preserved.";return false;}
        if(!battlefield_checkpoint_validate_fn(checkpoint,error))return false;
    }
    // Checkpoint progress may rewind; belongings, purchases and receipts do not.
    inventory_pending_restore=true;return true;
};
inventory_commit_restore=[&](){
    if(!inventory_pending_restore)return;
    inventory_pending_restore=false;inventory_sync_story();player_inventory=inventory_profile_view();
    inventory_apply_weapon();inventory_open=false;inventory_input_gate.reset();inventory_controls_ready=false;
};
auto inventory_save=[&](){
    auto next=player_profile;next.inventory=player_inventory;
    // Quest tokens are projected from the current checkpoint, never traded.
    world_player_profile::strip_quest(next.inventory);
    next.inventory.receipts|=player_profile.inventory.receipts;
    if(!arena_enabled)next.inventory.counts[unsigned(InvItem::Bow)]=player_profile.inventory.counts[unsigned(InvItem::Bow)];
    if(!arena_enabled&&next.inventory.weapon==InvItem::None)next.inventory.weapon=player_profile.inventory.weapon;
    if(!player_profile_commit(next,inventory_message)){++inventory_save_failures;return false;}
    if(arena_enabled)arena_progress=player_profile.arena;
    return true;
};
auto inventory_transaction=[&](const world_player_inventory::State &next,float health,float stamina,const std::string &message){
    const auto before=player_inventory;const float old_health=fight.health,old_stamina=fight.stamina;
    if(!world_player_inventory::valid(next))return false;
    player_inventory=next;fight.health=health;fight.stamina=stamina;
    if(!inventory_save()){player_inventory=before;fight.health=old_health;fight.stamina=old_stamina;return false;}
    ++inventory_transactions;inventory_message=message;return true;
};
inventory_select_weapon=[&](world_weapon_loadout::Weapon weapon){
    if(!inventory_enabled)return true;
    if(!world_player_inventory::owns(player_inventory,weapon)){playtest_tell("You do not own that weapon yet.");inventory_message="That weapon is not in your backpack.";return false;}
    auto next=player_inventory;std::string message;
    if(!world_player_inventory::equip(next,world_player_inventory::weapon_item(weapon),message))return false;
    if(next.weapon==player_inventory.weapon)return true;
    return inventory_transaction(next,fight.health,fight.stamina,message);
};
auto inventory_release_controls=[&](){
    opening_release();arena_release();sword_stick.reset();fight.clear_buffer();fight.set_guard(false);
    opening_confirm=opening_interact=opening_enter=opening_cancel=false;
    inventory_input_gate.reset();inventory_controls_ready=false;
};
auto inventory_close=[&](){
    if(!inventory_open)return;
    inventory_open=false;preview_screen=inventory_return_screen;arena_focus_panel=-1;opening_panel.clear();inventory_release_controls();
};
auto inventory_can_open=[&](){
    if(battlefield_is_active())return false;
    if(!inventory_enabled||show_graphics||destination_loading||studio.open||preview_screen==0)return false;
    if(first_journey_enabled&&(opening_frame.cinematic||first_journey_dialogue_active||opening_state.camp().active()||first_journey_defeated||opening_state.stage()==world_opening_prologue::Stage::GameMenu))return false;
    return fight.player_phase==TrainingPlayerPhase::Ready;
};
inventory_menu_open=[&](){
    if(inventory_open){inventory_close();return;}
    if(!inventory_can_open()){playtest_tell("Finish the current action before opening your backpack.");return;}
    inventory_return_screen=preview_screen;preview_screen=6;inventory_open=true;inventory_focus=true;++inventory_opened;
    inventory_message.clear();inventory_release_controls();
};
auto inventory_primary=[&](){
    if(!inventory_open)return;
    auto next=player_inventory;std::string message;const auto &d=world_player_inventory::definition(inventory_selected);
    if(d.kind==InvKind::Weapon){
        const auto w=world_player_inventory::weapon_type(inventory_selected);
        if(w==world_weapon_loadout::Weapon::Bow&&!arena_enabled){inventory_message="This bow is loaned for arena practice.";return;}
        if(unsigned(w)>=2&&!world_combat_motion_loadouts_ready(combat_motion)){inventory_message="Weapon assets are unavailable.";return;}
        if(equipment_request_weapon&&equipment_request_weapon(w)){inventory_message=equipment_request_feedback.message;}
        else if(inventory_select_weapon(w)){inventory_apply_weapon();inventory_message=std::string(inventory_item_name(inventory_selected))+" equipped.";}
    }else if(d.kind==InvKind::Armor){
        if(!world_player_inventory::equip(next,inventory_selected,message)){inventory_message=message;return;}
        inventory_transaction(next,fight.health,fight.stamina,message);
    }else if(d.kind==InvKind::Food){
        float hp=fight.health,sp=fight.stamina;
        if(!world_player_inventory::use(next,inventory_selected,hp,sp,message)){inventory_message=message;return;}
        inventory_transaction(next,hp,sp,message);
    }else inventory_message="Quest items are handed over through the story.";
};
auto inventory_unequip=[&](){
    auto next=player_inventory;std::string message;
    if(!world_player_inventory::unequip(next,inventory_selected,message)){inventory_message=message;return;}
    inventory_transaction(next,fight.health,fight.stamina,message);
};
auto inventory_loot_near=[&](){
    return first_journey_enabled&&!battlefield_is_active()&&first_journey_outdoors()&&first_journey_state.encounter_won()&&
        !(player_inventory.receipts&world_player_inventory::loot_receipt)&&!first_journey_dialogue_active&&
        !fight.engaged()&&!cam.swimming&&!cam.fly&&std::abs(cam.y-1.75-first_journey_enemy_feet)<2&&
        std::hypot(cam.x-first_journey_enemy_x,cam.z-first_journey_enemy_z)<2.6;
};
auto inventory_take_loot=[&](){
    if(!inventory_loot_near())return false;
    // This press belongs to looting, including a rejected full-backpack/save
    // attempt. The story also reads raw controller A; rearm it after release.
    first_journey_state.release_interaction();
    auto next=player_inventory;std::string message;
    if(!world_player_inventory::loot_raider(next,message)){inventory_message=message;opening_tell(message);}
    else if(inventory_transaction(next,fight.health,fight.stamina,message)){++inventory_loot_pickups;opening_tell(message+"  I / View: inventory.");}
    else opening_tell(inventory_message.empty()?"The items could not be saved. Try again.":inventory_message);
    // Stop the same E/A press from also collecting the nearby quest satchel.
    opening_interact=opening_confirm=opening_enter=false;opening_release();return true;
};
auto inventory_key=[&](SDL_Keycode key){
    if(!inventory_enabled)return false;
    if(key==SDLK_i){inventory_menu_open();return true;}
    if(inventory_open){
        if(key==SDLK_ESCAPE)inventory_close();
        else if(key==SDLK_x)inventory_primary();
        else if(key==SDLK_u)inventory_unequip();
        return true;
    }
    if(!inventory_controls_ready){opening_confirm=opening_interact=opening_enter=false;return true;}
    if(key==SDLK_e&&preview_screen==1&&inventory_controls_ready&&inventory_take_loot())return true;
    return false;
};
auto inventory_controller=[&](SDL_GameControllerButton button){
    if(!inventory_enabled)return false;
    if(button==SDL_CONTROLLER_BUTTON_BACK){inventory_menu_open();return true;}
    if(inventory_open){
        if(button==SDL_CONTROLLER_BUTTON_B||button==SDL_CONTROLLER_BUTTON_START)inventory_close();
        else if(button==SDL_CONTROLLER_BUTTON_X)inventory_primary();
        else if(button==SDL_CONTROLLER_BUTTON_Y)inventory_unequip();
        else if(button==SDL_CONTROLLER_BUTTON_LEFTSHOULDER||button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER){inventory_filter=(inventory_filter+(button==SDL_CONTROLLER_BUTTON_LEFTSHOULDER?3:1))%4;inventory_focus=true;}
        return true;
    }
    if(!inventory_controls_ready)return true;
    if(button==SDL_CONTROLLER_BUTTON_A&&preview_screen==1&&inventory_controls_ready&&inventory_take_loot())return true;
    return false;
};
auto inventory_tick=[&](){
    if(!inventory_enabled)return;
    if(first_journey_enabled){
        inventory_sync_story();
        if(player_inventory.weapon!=InvItem::None&&world_player_inventory::weapon_type(player_inventory.weapon)!=story_weapon)inventory_apply_weapon();
    }
    world_combat_motion_equipment(combat_motion,world_player_inventory::appearance(player_inventory));
    // The marked, bounded intro replay receives tagged SDL focus events while
    // running offscreen. Real sessions continue to require OS window focus.
    if(!inventory_open&&!inventory_controls_ready)inventory_controls_ready=inventory_input_gate.observe(preview_pad.focused&&((SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)||npc_intro_qa||road_traffic_qa||equipment_qa),arena_raw_held()||SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_I]);
};
// Vector icons keep the menu crisp at every supported display scale and avoid
// adding texture uploads to the existing three-gigabyte graphics budget.
auto inventory_icon=[&](ImDrawList *dl,ImVec2 c,float r,InvItem item,ImU32 color){
    const auto k=world_player_inventory::definition(item).kind;
    if(k==InvKind::Weapon){
        if(item==InvItem::SwordShield){dl->AddQuad({c.x-r,c.y-r*.65f},{c.x-r*.1f,c.y-r*.65f},{c.x-r*.15f,c.y+r*.25f},{c.x-r*.55f,c.y+r*.75f},color,2);}
        if(item==InvItem::Bow){dl->PathArcTo(c,r,-1.2f,1.2f,18);dl->PathStroke(color,0,2);dl->AddLine({c.x+r*.36f,c.y-r*.93f},{c.x+r*.36f,c.y+r*.93f},color,1);}
        else{dl->AddLine({c.x-r*.6f,c.y+r*.85f},{c.x+r*.65f,c.y-r*.9f},color,item==InvItem::Staff?4:3);if(item==InvItem::Axe)dl->AddTriangleFilled({c.x,c.y-r*.85f},{c.x+r,c.y-r*.55f},{c.x+r*.5f,c.y},color);else if(item!=InvItem::Staff)dl->AddLine({c.x-r*.65f,c.y+r*.2f},{c.x+r*.05f,c.y+r*.7f},color,2);}
    }else if(k==InvKind::Armor){dl->AddPolyline(std::array<ImVec2,8>{{{c.x-r,c.y-r*.5f},{c.x-r*.45f,c.y-r},{c.x,c.y-r*.6f},{c.x+r*.45f,c.y-r},{c.x+r,c.y-r*.5f},{c.x+r*.65f,c.y+r},{c.x-r*.65f,c.y+r},{c.x-r,c.y-r*.5f}}}.data(),8,color,ImDrawFlags_None,2);}
    else if(k==InvKind::Food){if(item==InvItem::Potion){dl->AddRect({c.x-r*.45f,c.y-r*.3f},{c.x+r*.45f,c.y+r*.8f},color,r*.2f,0,2);dl->AddRect({c.x-r*.25f,c.y-r*.9f},{c.x+r*.25f,c.y-r*.3f},color,0,0,2);dl->AddLine({c.x-r*.3f,c.y+r*.25f},{c.x+r*.3f,c.y+r*.25f},color,3);}else{dl->AddCircle({c.x,c.y+r*.1f},r*.7f,color,14,2);dl->AddLine({c.x,c.y-r*.6f},{c.x+r*.25f,c.y-r},color,2);}}
    else{dl->AddRect({c.x-r*.65f,c.y-r*.85f},{c.x+r*.65f,c.y+r*.85f},color,2,0,2);for(int n=0;n<3;++n)dl->AddLine({c.x-r*.4f,c.y+float(n-1)*r*.4f},{c.x+r*.4f,c.y+float(n-1)*r*.4f},color,1);}
};
auto inventory_ui=[&](bool loading){
    if(!inventory_enabled||loading||show_graphics)return;
    auto &io=ImGui::GetIO();const float w=io.DisplaySize.x,h=io.DisplaySize.y;
    const float u=std::clamp(std::min(w/1600.f,h/900.f),.55f,1.5f);
    if(!inventory_open){
        if(preview_screen==1&&(!opening_enabled||opening_controls_enabled)){
            auto *d=ImGui::GetForegroundDrawList();const char *hint=preview_pad.using_pad?"View: inventory":"I: inventory";
            d->AddText(ImGui::GetFont(),13*u,{w-172*u,h-24*u},IM_COL32(227,214,185,220),hint);
            if(inventory_loot_near()){const std::string prompt=preview_pad.using_pad?"[A]  Search fallen raider":"[E]  Search fallen raider";const auto width=ImGui::CalcTextSize(prompt.c_str()).x;d->AddRectFilled({(w-width)*.5f-18*u,h-190*u},{(w+width)*.5f+18*u,h-150*u},IM_COL32(12,20,24,235),5*u);d->AddText({(w-width)*.5f,h-180*u},IM_COL32(241,205,135,255),prompt.c_str());}
        }return;
    }
    inventory_item_rects={};inventory_action_rect={};inventory_close_rect={};
    auto *bg=ImGui::GetBackgroundDrawList();bg->AddRectFilled({0,0},{w,h},IM_COL32(3,9,13,215));
    const ImVec4 gold{.91f,.77f,.49f,1},muted{.60f,.67f,.68f,1};
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{24*u,22*u});ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{10*u,10*u});ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{10*u,9*u});ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,10*u);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,{.035f,.065f,.08f,.98f});ImGui::PushStyleColor(ImGuiCol_ChildBg,{.045f,.078f,.09f,1});ImGui::PushStyleColor(ImGuiCol_Button,{.09f,.14f,.16f,1});ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{.18f,.25f,.27f,1});ImGui::PushStyleColor(ImGuiCol_ButtonActive,{.30f,.28f,.19f,1});ImGui::PushStyleColor(ImGuiCol_Border,{.30f,.32f,.28f,1});
    ImGui::SetNextWindowPos({w*.5f,h*.5f},ImGuiCond_Always,{.5f,.5f});ImGui::SetNextWindowSize({std::min(w-28*u,1340*u),std::min(h-30*u,780*u)},ImGuiCond_Always);
    if(inventory_focus)ImGui::SetNextWindowFocus();
    ImGui::Begin("Backpack###player-inventory",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetWindowFontScale(1.15f*u);ImGui::TextColored(gold,"EQUIPMENT  &  BELONGINGS");ImGui::SameLine();ImGui::SetCursorPosX(ImGui::GetWindowWidth()-142*u);
    if(ImGui::Button("Close  [B / Esc]"))inventory_close();
    {auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();inventory_close_rect={a.x,a.y,b.x,b.y};}
    ImGui::SetWindowFontScale(u);ImGui::TextColored(muted,"%s",arena_enabled?"ARENA  /  Your equipment and winnings carry into the story":"THE MEADOWS OF VALOIS  /  One character, one purse");
    ImGui::Text("%s   |   %u / %u slots   |   %.1f kg   |   Health %.0f / 100   |   Stamina %.0f / 100",world_currency::format(world_player_inventory::purse(player_inventory,inventory_earned())).c_str(),world_player_inventory::occupied(player_inventory),world_player_inventory::capacity,world_player_inventory::weight(player_inventory),fight.health,fight.stamina);
    ImGui::Separator();
    const float full=ImGui::GetContentRegionAvail().x,left=full*.23f,right=full*.29f,gap=20*u,mid=full-left-right-gap;
    const float content=std::max(260*u,ImGui::GetContentRegionAvail().y-94*u);
    ImGui::BeginChild("Equipment",{left,content},ImGuiChildFlags_Borders);
    ImGui::TextColored(gold,"EQUIPPED");ImGui::Separator();
    auto equipment=[&](const char *slot,InvItem item){ImGui::TextColored(muted,"%s",slot);ImGui::PushID(slot);if(ImGui::Button(item==InvItem::None?"Empty":inventory_item_name(item),{-1,34*u})){inventory_selected=item;inventory_filter=0;}ImGui::PopID();};
    equipment("MAIN HAND",player_inventory.weapon);
    ImGui::TextColored(muted,"OFF HAND");ImGui::TextWrapped("%s",player_inventory.weapon==InvItem::SwordShield?"Oak shield":player_inventory.weapon==InvItem::Bow?"Arrow nocked when drawing":player_inventory.weapon==InvItem::None?"Empty":"Two-handed grip");
    const char *slots[]={"BODY","HEAD","HANDS","FEET"};for(unsigned k=0;k<4;++k)equipment(slots[k],player_inventory.armor[k]);
    ImGui::Separator();ImGui::Text("Damage reduction: %.0f%%",100*world_player_inventory::protection(player_inventory));
    ImGui::EndChild();ImGui::SameLine();
    ImGui::BeginChild("Backpack",{mid,content},ImGuiChildFlags_Borders);
    const char *filters[]={"All","Gear","Food","Quest"};for(int f=0;f<4;++f){if(f)ImGui::SameLine();const bool active=f==inventory_filter;if(active)ImGui::PushStyleColor(ImGuiCol_Button,{.28f,.25f,.16f,1});if(ImGui::Button(filters[f])){inventory_filter=f;inventory_focus=true;}if(active)ImGui::PopStyleColor();}
    ImGui::Separator();bool first=true;unsigned visible=0;
    const float tile=(ImGui::GetContentRegionAvail().x-20*u)/3.f;
    for(unsigned n=1;n<unsigned(InvItem::Count);++n){
        const InvItem item=InvItem(n);const auto &d=world_player_inventory::definition(item);const unsigned amount=world_player_inventory::count(player_inventory,item);
        if(!amount||(inventory_filter==1&&d.kind!=InvKind::Weapon&&d.kind!=InvKind::Armor)||(inventory_filter==2&&d.kind!=InvKind::Food)||(inventory_filter==3&&d.kind!=InvKind::Quest))continue;
        if(visible%3)ImGui::SameLine();
        ImGui::PushID(int(n));
        const bool selected=inventory_selected==item;if(selected)ImGui::PushStyleColor(ImGuiCol_Button,{.25f,.24f,.16f,1});
        if(ImGui::Button("###item",{tile,91*u}))inventory_selected=item;
        if(selected)ImGui::PopStyleColor();
        if(inventory_focus&&first){ImGui::SetItemDefaultFocus();ImGui::SetKeyboardFocusHere(-1);first=false;}
        if(ImGui::IsItemFocused())inventory_selected=item;
        const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();inventory_item_rects[n]={a.x,a.y,b.x,b.y};
        auto *dl=ImGui::GetWindowDrawList();const ImU32 color=d.kind==InvKind::Quest?IM_COL32(232,195,113,255):d.rarity[0]=='U'?IM_COL32(130,204,166,255):IM_COL32(215,221,206,255);
        inventory_icon(dl,{a.x+24*u,a.y+25*u},14*u,item,color);dl->AddText(ImGui::GetFont(),12*u,{a.x+45*u,a.y+13*u},color,d.rarity);
        const char *item_name=inventory_item_name(item);std::string line=item_name;const float maxw=tile-16*u;while(line.size()>5&&ImGui::GetFont()->CalcTextSizeA(15*u,FLT_MAX,0,line.c_str()).x>maxw)line.resize(line.size()-1);if(line!=item_name)line+="...";
        dl->AddText(ImGui::GetFont(),15*u,{a.x+8*u,a.y+48*u},IM_COL32(239,237,221,255),line.c_str());
        const std::string status=world_player_inventory::equipped(player_inventory,item)?"EQUIPPED":amount>1?"x"+std::to_string(amount):"";
        dl->AddText(ImGui::GetFont(),13*u,{a.x+8*u,a.y+70*u},IM_COL32(220,186,119,255),status.c_str());
        if(ImGui::IsItemHovered()){ImGui::SetNextWindowSize({300*u,0},ImGuiCond_Always);ImGui::BeginTooltip();ImGui::TextWrapped("%s",inventory_item_name(item));ImGui::TextWrapped("%s",d.description);ImGui::EndTooltip();}
        ImGui::PopID();++visible;
    }
    if(!visible)ImGui::TextWrapped("%s",inventory_filter?"No items in this category.":"Your backpack is empty. Speak with the travelers at the fire to begin your journey.");
    ImGui::EndChild();ImGui::SameLine();
    ImGui::BeginChild("Item details",{0,content},ImGuiChildFlags_Borders);
    const auto &d=world_player_inventory::definition(inventory_selected);const bool owned=world_player_inventory::count(player_inventory,inventory_selected)>0;
    ImGui::TextColored(gold,"ITEM DETAILS");ImGui::Separator();
    if(owned){
        ImGui::TextColored(muted,"%s",d.rarity);ImGui::SetWindowFontScale(1.18f*u);ImGui::TextWrapped("%s",inventory_item_name(inventory_selected));ImGui::SetWindowFontScale(u);ImGui::TextWrapped("%s",d.description);ImGui::Text("Quantity: %u   |   %.2f kg each",world_player_inventory::count(player_inventory,inventory_selected),d.weight);ImGui::Separator();
        if(d.kind==InvKind::Weapon){
            const auto weapon=world_player_inventory::weapon_type(inventory_selected);
            const auto &p=world_weapon_loadout::profile(weapon);
            if(weapon==world_weapon_loadout::Weapon::Bow){
                ImGui::TextUnformatted("Ranged / two-handed");
                ImGui::Text("Full draw: %.1f seconds",world_arena_combat::Arena::maximum_draw);
                ImGui::TextUnformatted("Damage and stamina depend on draw.");
                ImGui::TextWrapped("Hold RT / left mouse, then release. Physical arrows follow gravity. Practice arrows are supplied.");
            }else{
                if(player_inventory.weapon==InvItem::Bow||player_inventory.weapon==InvItem::None){ImGui::Text("Damage: x%.2f",p.damage);ImGui::Text("Reach: %.2f m",p.reach);}
                else{const auto &old=world_weapon_loadout::profile(world_player_inventory::weapon_type(player_inventory.weapon));ImGui::Text("Damage: x%.2f (%+.2f)",p.damage,p.damage-old.damage);ImGui::Text("Reach: %.2f m (%+.2f)",p.reach,p.reach-old.reach);}
                ImGui::Text("Swing time: x%.2f",p.speed);ImGui::Text("Stamina / cut: %.1f",16*p.stamina);ImGui::Text("Block cost: %.0f",p.block_cost);ImGui::TextWrapped("%s",p.description);
            }
        }else if(d.kind==InvKind::Armor){auto old=player_inventory.armor[unsigned(d.slot)];ImGui::Text("Protection: %.0f%% (%+.0f%%)",d.protection*100,(d.protection-world_player_inventory::definition(old).protection)*100);ImGui::TextColored(muted,"Compared with equipped %s",slots[unsigned(d.slot)]);}
        if(d.kind!=InvKind::Quest){const bool eq=world_player_inventory::equipped(player_inventory,inventory_selected);ImGui::BeginDisabled(eq&&d.kind!=InvKind::Food);
            if(ImGui::Button(d.kind==InvKind::Food?"Use selected  [X]":eq?"Equipped":"Equip selected  [X]",{-1,42*u}))inventory_primary();
            {auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();inventory_action_rect={a.x,a.y,b.x,b.y};}ImGui::EndDisabled();
            if(eq&&d.kind==InvKind::Armor&&ImGui::Button("Unequip  [Y / U]",{-1,35*u}))inventory_unequip();
        }else ImGui::TextColored(gold,"Protected story item");
    }else ImGui::TextWrapped("Select an item to compare, equip or use it.");
    const bool merchant=(arena_enabled&&arena_ui_phase!=ArenaUiPhase::Fight)||(first_journey_enabled&&first_journey_state.town_reached()&&std::hypot(cam.x-first_journey_state.config().town.x,cam.z-first_journey_state.config().town.z)<6.);
    if(merchant){ImGui::Separator();ImGui::TextColored(gold,arena_enabled?"ARENA QUARTERMASTER":"TOWN MERCHANT");for(auto item:{InvItem::Bread,InvItem::Apple,InvItem::Potion,InvItem::Mail,InvItem::Helmet,InvItem::Gloves,InvItem::Boots}){const auto label=std::string("Buy ")+world_player_inventory::definition(item).name+" / "+std::to_string(world_player_inventory::price(item))+"c";if(ImGui::Button(label.c_str(),{-1,30*u})){auto next=player_inventory;std::string m;if(world_player_inventory::buy(next,item,inventory_earned(),m))inventory_transaction(next,fight.health,fight.stamina,m);else inventory_message=m;}}}
    ImGui::EndChild();
    ImGui::Separator();if(!inventory_message.empty())ImGui::TextWrapped("%s",inventory_message.c_str());
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextColored(muted,"Arrows / left stick: navigate   A / Enter: select   X: equip / use   Y: unequip   LB / RB: categories   B / Esc: close");
    ImGui::TextColored(muted,"100 copper = 1 silver   |   100 silver = 1 gold   |   Equipment and used items save automatically");
    ImGui::PopTextWrapPos();
    inventory_focus=false;ImGui::End();ImGui::PopStyleColor(6);ImGui::PopStyleVar(4);
};

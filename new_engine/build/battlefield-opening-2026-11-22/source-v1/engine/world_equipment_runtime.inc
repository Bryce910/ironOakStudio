// Host boundaries for owned relaxed-stance presentation. This does not author
// a scabbard/grab animation or reinterpret Bow charge as equipment movement.
equipment_request_weapon=[&](world_weapon_loadout::Weapon weapon){
    equipment_request_feedback.clear();
    if(!skeletal_hero_loaded||!skeletal_hero.supports_equipment())return false;
    using Request=world_equipment_presentation::RequestResult;
    if(!world_weapon_loadout::valid(weapon)){equipment_request_feedback.set(Request::Rejected,"That weapon is unavailable.");return true;}
    if(fight.player_phase!=TrainingPlayerPhase::Ready||equipment_change.active()){
        equipment_request_feedback.set(Request::Rejected,"Finish the current action before changing equipment.");
        playtest_tell(equipment_request_feedback.message);return true;
    }
    if(inventory_enabled&&!world_player_inventory::owns(player_inventory,weapon)){
        equipment_request_feedback.set(Request::Rejected,"That weapon is not in your backpack.");
        playtest_tell(equipment_request_feedback.message);return true;
    }
    if(weapon==fight.weapon){
        if(weapon_sheathed){equipment_request_draw();equipment_request_feedback.set(Request::DrawRequested,"Drawing the equipped weapon. Close the backpack to continue.");}
        else equipment_request_feedback.set(Request::AlreadyEquipped,"That weapon is already equipped.");
        return true;
    }
    equipment_action_gate.reset();fight.set_guard(false);fight.clear_buffer();sword_stick.reset();
    equipment_change.request(unsigned(weapon));
    equipment_request_feedback.set(Request::Queued,"Weapon change queued. Close the backpack to lower and equip.");
    // Existing Bow charge returns to its owned neutral pose before the hidden
    // boundary. There is no dedicated Bow draw/stow capture in this pack.
    if(fight.weapon!=world_weapon_loadout::Weapon::Bow)weapon_sheathed=true;
    arena_game.cancel_draw();arena_mouse_draw=arena_mouse_aim=false;arena_bow_armed=false;
    playtest_tell("Lowering equipment...");return true;
};
equipment_tick=[&](float delta,bool paused){
    if(!equipment_change.active())return;
    if(fight.health<=0||fight.state==TrainingState::Lost){equipment_cancel_pending();return;}
    const bool bow=fight.weapon==world_weapon_loadout::Weapon::Bow;
    const bool lowered=!bow&&skeletal_hero_loaded&&skeletal_hero.equipment_stowed_settled();
    const bool ready=!bow&&skeletal_hero_loaded&&skeletal_hero.equipment_combat_ready();
    const auto action=equipment_change.advance(delta,paused,bow,lowered,ready);
    using Action=world_equipment_presentation::ChangeAction;
    if(action==Action::Commit){
        const auto next=world_weapon_loadout::Weapon(equipment_change.target());
        if(inventory_select_weapon&&!inventory_select_weapon(next)){
            equipment_change.committed(false);weapon_sheathed=false;equipment_action_gate.reset();return;
        }
        if(arena_enabled)arena_game.set_weapon(next);else story_weapon=next;
        fight.weapon=next;weapon_sheathed=true;fight.set_guard(false);fight.clear_buffer();sword_stick.reset();
        if(next==world_weapon_loadout::Weapon::Bow)arena_target_lock.clear();
        arena_game.cancel_draw();arena_mouse_draw=arena_mouse_aim=false;arena_bow_armed=arena_bow_held_before=false;
        equipment_change.committed(true);equipment_action_gate.reset();
    }else if(action==Action::BeginDraw){
        weapon_sheathed=false;++weapon_draws;equipment_action_gate.reset();
        playtest_tell(std::string(world_weapon_loadout::profile(fight.weapon).name)+" raising...");
    }else if(action==Action::Completed){
        equipment_action_gate.reset();playtest_tell(std::string(world_weapon_loadout::profile(fight.weapon).name)+" ready.");
    }
};

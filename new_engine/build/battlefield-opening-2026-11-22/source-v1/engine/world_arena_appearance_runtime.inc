// Loaded after the two cosmetic catalogs. All allocation happens between runs.
struct ArenaAppearanceBank {
    std::string id;bool orc=false;unsigned mask=0;
    std::shared_ptr<WorldCombatMotion> motion;
};
std::vector<ArenaAppearanceBank> arena_appearance_banks;
std::vector<std::string> arena_enemy_appearances;
std::array<WorldCombatMotion*,world_combat_capacity::npcs> arena_actor_banks={};
std::vector<std::string> arena_ally_appearances;
WorldCombatMotion *arena_player_bank=nullptr;
std::uint64_t arena_appearance_seed=0;
unsigned arena_appearance_runs=0;
auto arena_each_appearance=[&](auto &&callback){for(auto &bank:arena_appearance_banks)callback(bank.motion.get());};
auto arena_prepare_appearances=[&](world_arena_opponents::Kind kind,unsigned total,unsigned teammates,
                                  const std::string &player,std::string &error){
    if(!total||total>world_combat_capacity::opponents||teammates>world_combat_capacity::teammates){error="Choose 1–24 opponents and 0–6 teammates.";return false;}
    if(!arena_compact_roster_enabled){error="This arena needs the complete character roster before a bout can start.";return false;}
    std::vector<std::string> ids;
    if(kind==world_arena_opponents::Kind::Orc){for(const auto &entry:arena_orc_roster.entries)ids.push_back(entry.id);}
    else for(unsigned i=1;i<=18;++i){char name[16];std::snprintf(name,sizeof name,"knight%02u",i);ids.emplace_back(name);}
    std::random_device entropy;const auto seed=(std::uint64_t(entropy())<<32)^entropy()^std::uint64_t(arena_appearance_runs+1);
    std::vector<std::string> assignments;
    if(!world_arena_appearance_assignment::make(ids,total,seed,assignments)){
        error="The selected opponent roster is incomplete.";return false;
    }
    if(launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_BLADE_CLASH")){
        std::ifstream marker(".valwood-blade-clash-qa");std::string token;std::getline(marker,token);
        if(token=="VALWOOD_PRIVATE_BLADE_CLASH_V1")if(const char *target=std::getenv("VALWOOD_QA_CLASH_ENEMY")){
            if(total!=1||std::find(ids.begin(),ids.end(),target)==ids.end()){error="Private clash fixture is not in the selected roster.";return false;}
            assignments.assign(1,target);
        }
    }
    // Bounded, privately marked material regression rehearsal. Keep normal
    // opponent randomness in public play; only choose the requested owned
    // source appearance, never its guard, health or collision outcome.
    if(kind==world_arena_opponents::Kind::Orc&&launch->frames&&!launch->benchmark&&
       std::getenv("VALWOOD_QA_ARENA_ROSTER")&&std::getenv("VALWOOD_QA_ARENA_APPROACH")){
        std::ifstream marker(".valwood-arena-roster-qa");std::string token;std::getline(marker,token);
        if(token=="VALWOOD_PRIVATE_ARENA_ROSTER_V1")if(const char *target=std::getenv("VALWOOD_QA_APPROACH_ORC_ID")){
            if(total!=1||std::find(ids.begin(),ids.end(),target)==ids.end()){error="Private approach fixture is not in the selected roster.";return false;}
            assignments.assign(1,target);
        }
    }
    // Posed, frame-limited exposed-face material comparison. This selector
    // is unreachable in public play and changes only the source appearance.
    if(kind!=world_arena_opponents::Kind::Orc&&launch->frames&&!launch->benchmark&&
       std::getenv("VALWOOD_QA_ARENA_ROSTER")&&std::getenv("VALWOOD_QA_KNIGHT_PRESENTATION")){
        std::ifstream marker(".valwood-arena-roster-qa");std::string token;std::getline(marker,token);
        if(token=="VALWOOD_PRIVATE_ARENA_ROSTER_V1")if(const char *target=std::getenv("VALWOOD_QA_PRESENTATION_KNIGHT_ID")){
            if(total!=1||std::find(ids.begin(),ids.end(),target)==ids.end()){error="Private knight face fixture is not in the selected roster.";return false;}
            assignments.assign(1,target);
        }
    }
    std::vector<std::string> knight_ids,allies;
    for(unsigned i=1;i<=18;++i){char name[16];std::snprintf(name,sizeof name,"knight%02u",i);knight_ids.emplace_back(name);}
    if(teammates&&!world_arena_appearance_assignment::make(knight_ids,teammates,seed^0x9e3779b97f4a7c15ull,allies)){
        error="The allied knight roster is incomplete.";return false;
    }
    struct Request {std::string id;bool orc;unsigned mask;};std::vector<Request> requests;
    auto request=[&](const std::string &id,bool orc,unsigned bit){
        for(auto &entry:requests)if(entry.id==id&&entry.orc==orc){entry.mask|=bit;return;}
        requests.push_back({id,orc,bit});
    };
    request(player,false,1);
    for(unsigned i=0;i<assignments.size();++i)request(assignments[i],kind==world_arena_opponents::Kind::Orc,1u<<(i+1));
    for(unsigned i=0;i<allies.size();++i)request(allies[i],false,1u<<(world_combat_capacity::opponents+i+1));
    // This is a between-bout operation. Release the completed roster before
    // allocating the next, so changing species cannot double VRAM residency.
    // A failed import returns to setup; it never allocates a reward/run receipt.
    arena_appearance_banks.clear();arena_actor_banks.fill(nullptr);
    arena_player_bank=nullptr;arena_enemy_appearances.clear();arena_ally_appearances.clear();
    std::vector<ArenaAppearanceBank> staged;
    for(const auto &entry:requests){
        std::string body,anatomy,packet;
        if(entry.orc){
            const int index=arena_orc_roster.index(entry.id);
            if(index<0){error="An Orc character is unavailable.";return false;}
            const auto &source=arena_orc_roster.entries[unsigned(index)];
            body="arena_orcs_roster/"+source.body_directory;anatomy="arena_orcs_roster/"+source.anatomy_directory;
            packet="compact_orcs/"+entry.id+"/poses.pnt";
        }else{body="npc_knights/"+entry.id+"/body";anatomy="npc_knights/"+entry.id+"/anatomy";packet="npc_knights/"+entry.id+"/poses.pnt";}
        const unsigned requested=unsigned(resident_character_textures);
        const unsigned resident=(entry.mask&1)||requests.size()<=3?requested:requests.size()>8?0u:std::min(1u,requested);
        world_character_textures::LoadingQuality texture_scope(resident);
        std::printf("[Character textures] %s: %u px color (%s)\n",entry.id.c_str(),512u<<resident,(entry.mask&1)?"player":"crowd budget");
        char detail[512]={};auto *motion=world_combat_motion_load_compact(body.c_str(),anatomy.c_str(),packet.c_str(),entry.mask,entry.orc,detail,sizeof detail,entry.orc&&total+teammates>8?512u:0u);
        if(!motion){error="Could not prepare "+entry.id+": "+detail;return false;}
        std::shared_ptr<WorldCombatMotion> owner(motion,world_combat_motion_free);
        world_combat_motion_fatality_ground(motion,[](double,double,void*){return 64.;},nullptr);
        staged.push_back({entry.id,entry.orc,entry.mask,std::move(owner)});
    }
    std::array<WorldCombatMotion*,world_combat_capacity::npcs> actors={};WorldCombatMotion *hero=nullptr;
    for(auto &bank:staged){
        if(bank.mask&1)hero=bank.motion.get();
        for(unsigned i=0;i<world_combat_capacity::npcs;++i)if(bank.mask&(1u<<(i+1)))actors[i]=bank.motion.get();
    }
    if(!hero||std::any_of(actors.begin(),actors.begin()+total,[](auto *p){return !p;})||
       std::any_of(actors.begin()+world_combat_capacity::opponents,actors.begin()+world_combat_capacity::opponents+teammates,[](auto *p){return !p;})){
        error="The arena character assignment is incomplete.";return false;
    }
    arena_appearance_banks=std::move(staged);arena_actor_banks=actors;arena_player_bank=hero;
    arena_enemy_appearances=std::move(assignments);arena_ally_appearances=std::move(allies);arena_appearance_seed=seed;++arena_appearance_runs;
    world_combat_motion_set_actor_mask(combat_motion,1);
    for(unsigned i=0;i<arena_enemy_appearances.size();++i)
        std::printf("[Arena appearance] run=%u opponent=%u kind=%s character=%s\n",arena_appearance_runs,i,
                    kind==world_arena_opponents::Kind::Orc?"Orc":"Knight",arena_enemy_appearances[i].c_str());
    error.clear();return true;
};

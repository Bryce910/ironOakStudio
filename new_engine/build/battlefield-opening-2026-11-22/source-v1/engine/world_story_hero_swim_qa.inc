// Opt-in physical shoreline route, driven entirely by the owned SDL pad.
// Two runs bracket Bram's loan; this never assigns a player/story/health pose.
const bool story_swim_qa_enabled=opening_qa&&std::getenv("VALWOOD_QA_STORY_SWIM");
const bool story_swim_qa_only=story_swim_qa_enabled&&std::getenv("VALWOOD_QA_STORY_SWIM_ONLY");
// A shallow creek is a valid surface-swim fixture, never evidence of diving.
// Deep-water acceptance opts in explicitly and fails closed on a shallow end.
const bool story_swim_qa_require_dive=story_swim_qa_enabled&&std::getenv("VALWOOD_QA_STORY_SWIM_REQUIRE_DIVE");
std::array<bool,2> story_swim_qa_done={};
std::array<bool,2> story_swim_qa_dove={};
std::array<bool,2> story_swim_qa_float_checked={},story_swim_qa_hold_checked={},story_swim_qa_render_underwater_checked={};
std::array<unsigned,2> story_swim_qa_float_samples={},story_swim_qa_hold_samples={};
std::array<double,2> story_swim_qa_max_float_error={},story_swim_qa_max_hold_drift={};
std::array<unsigned,2> story_swim_qa_steps={};
std::array<double,2> story_swim_qa_walked={};
std::vector<world_opening_prologue::Vec> story_swim_qa_out,story_swim_qa_back;
bool story_swim_qa_loaded=false;unsigned story_swim_qa_run=0,story_swim_qa_phase=0,story_swim_qa_node=0;
uint64_t story_swim_qa_started=0,story_swim_qa_action=0,story_swim_qa_color_begin=0,story_swim_qa_fp_begin=0;
double story_swim_qa_x=0,story_swim_qa_z=0,story_swim_qa_hold_eye=0;
auto story_swim_qa_tick=[&](uint64_t now,bool loaned){
    if(!story_swim_qa_enabled||story_swim_qa_done[loaned?1:0]||!running)return;
    auto fail=[&](const char *why){opening_prologue_qa_fail(why);};
    auto neutral=[&](){opening_prologue_qa_axes(0,0);controller_test_pad.axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);controller_test_pad.axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);};
    if(!story_swim_qa_loaded){
        std::ifstream marker(".valwood-story-swim-qa"),file("opening/story-swim.qa.walk");std::string tag,extra;unsigned version=0;
        bool ok=bool(marker>>tag)&&tag=="VALWOOD_PRIVATE_STORY_SWIM_V1"&&!(marker>>extra)&&
            bool(file>>tag>>version)&&tag=="VALWOOD_STORY_SWIM_QA"&&version==1;
        for(auto *path:{&story_swim_qa_out,&story_swim_qa_back}){
            unsigned count=0;ok=ok&&bool(file>>tag>>count)&&tag=="path"&&count>=2&&count<=32;
            if(!ok)break;path->resize(count);for(auto &point:*path)ok=ok&&bool(file>>point.x>>point.y>>point.z)&&world_opening_prologue::detail::valid(point);
        }
        ok=ok&&bool(file>>tag)&&tag=="end"&&!(file>>extra)&&story_civilian_requested&&story_civilian_ready;
        if(!ok){fail("Story swim replay requires its marked bounded private shoreline route and civilian hero");return;}
        story_swim_qa_loaded=true;
        for(auto kind:{world_cover_collision::Kind::Trunk,world_cover_collision::Kind::Rock})
            for(const auto &o:generated_cover_collision.obstacles(kind))
                std::printf("[Story swim QA] Generated blocker %u %.9f %.9f y %.6f %.6f local %.6f %.6f %.6f %.6f rotation %.9f %.9f radius %.6f\n",unsigned(kind),o.x,o.z,o.ymin,o.ymax,o.xmin,o.xmax,o.zmin,o.zmax,o.cosine,o.sine,o.radius);
    }
    const unsigned run=loaned?1:0;
    if(!story_swim_qa_started||story_swim_qa_run!=run){
        story_swim_qa_run=run;story_swim_qa_phase=story_swim_qa_node=0;story_swim_qa_started=story_swim_qa_action=now;
        story_swim_qa_x=cam.x;story_swim_qa_z=cam.z;story_swim_qa_color_begin=story_swim_color_frames;story_swim_qa_fp_begin=story_swim_first_person_frames;neutral();return;
    }
    if(now>story_swim_qa_started+10500){fail("Physical shoreline swim replay exceeded its bounded input budget");return;}
    const double moved=std::hypot(cam.x-story_swim_qa_x,cam.z-story_swim_qa_z);
    if(moved>.7){fail("Unexpected capsule discontinuity during input-only swimming route");return;}
    story_swim_qa_walked[run]+=moved;story_swim_qa_x=cam.x;story_swim_qa_z=cam.z;++story_swim_qa_steps[run];
    if(story_swim_qa_steps[run]%600==0){std::printf("[Story swim QA] TRACE run%u phase%u node%u player %.6f %.6f %.6f\n",run,story_swim_qa_phase,story_swim_qa_node,cam.x,cam.y-1.75,cam.z);std::fflush(stdout);}
    if(first_journey_state.weapon_unlocked()!=loaned||story_appearance_armored){fail("Shoreline replay changed the loan state or the father's plain appearance");return;}
    float surface=0;const bool water=world_atlas_local_water(cam.x,cam.z,&surface,nullptr);
    const double support=walking_height(cam.x,cam.z,nullptr),depth=double(surface)-support;
    const bool submerged=cam.swimming&&water&&world_swimming::submerged(cam.y,surface);
    const bool outdoors=opening_state.stage()==world_opening_prologue::Stage::MeadowExplore||opening_state.stage()==world_opening_prologue::Stage::Complete;
    if(!outdoors){fail("Story swimming route left the actual outdoor Demo context");return;}
    const uint64_t age=now-story_swim_qa_action;
    auto next=[&](unsigned phase){story_swim_qa_phase=phase;story_swim_qa_action=now;neutral();};
    auto capture=[&](const char *name){opening_prologue_qa_capture_pending=std::string(loaned?"story-swim-loaned-":"story-swim-preloan-")+name;};
    auto walk=[&](const std::vector<world_opening_prologue::Vec> &path){
        if(story_swim_qa_node>=path.size()){neutral();return true;}
        if(!opening_movement_armed){neutral();return false;}
        const auto &point=path[story_swim_qa_node];const double dx=point.x-cam.x,dz=point.z-cam.z,distance=std::hypot(dx,dz);
        if(distance<.20){++story_swim_qa_node;neutral();return story_swim_qa_node>=path.size();}
        const double side=(dx*std::cos(cam.yaw)+dz*std::sin(cam.yaw))/distance,forward=(dx*std::sin(cam.yaw)-dz*std::cos(cam.yaw))/distance;
        const double speed=world_atlas_walk_speed()*(cam.swimming?world_swimming::speed_scale:1.);
        const double desired=std::clamp(distance/(speed/60.),.1,1.),raw=preview_settings.deadzone+(1-preview_settings.deadzone)*desired;
        opening_prologue_qa_axes(Sint16(std::clamp(side*raw,-1.,1.)*32767),Sint16(std::clamp(-forward*raw,-1.,1.)*32767));return false;
    };
    switch(story_swim_qa_phase){
    case 0:if(!walk(story_swim_qa_out))return;next(1);return;
    case 1:
        // One second allows genuine buoyancy and the source pose bridge to
        // settle even after walking down a deeper riverbed. Then observe one
        // complete additional second with neutral controls.
        if(age>=60){
            const double error=std::abs(cam.y-world_swimming::eye_target(surface,support));
            story_swim_qa_max_float_error[run]=std::max(story_swim_qa_max_float_error[run],error);++story_swim_qa_float_samples[run];
            if(!water||depth<=1.1||!cam.swimming||cam.grounded||error>.045){fail("Neutral creek floating did not settle above the actual supporting bed");return;}
        }
        if(age<120)return;
        if(!third_person||!skeletal_hero_active||!skeletal_hero.stats().swimming||skeletal_hero.stats().equipment_weapon_visible||skeletal_hero.equipment_combat_ready()||story_swim_color_frames<=story_swim_qa_color_begin){fail("Actual third-person swimming did not retain the plain hero, source swim capture and hidden weapon/contact");return;}
        story_swim_qa_float_checked[run]=story_swim_qa_float_samples[run]>=60;
        if(!story_swim_qa_float_checked[run]){fail("Neutral surface buoyancy did not receive its full observation window");return;}
        capture("surface-third");next(2);return;
    case 2:
        if(age<5)return;opening_prologue_qa_tap(SDLK_v,now);next(3);return;
    case 3:
        if(age<15)return;
        if(third_person||story_swim_first_person_frames<=story_swim_qa_fp_begin){fail("First-person swimming did not draw the civilian's own hands");return;}
        capture("surface-first");next(4);return;
    case 4:
        if(age<5)return;
        // This fixture may end in a shallow creek. Do not claim a dive when
        // the production standing capsule has no underwater headroom.
        if(depth<2.1){
            if(story_swim_qa_require_dive){fail("Required deep-water replay ended in water too shallow for the real capsule to dive");return;}
            opening_prologue_qa_pass(loaned?"loaned_shallow_creek_surface_only":"preloan_shallow_creek_surface_only");next(6);return;
        }
        controller_test_pad.axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT,32767);story_swim_qa_phase=5;story_swim_qa_action=now;return;
    case 5:
        if(!submerged||double(surface)-cam.y<.25){if(age>300)fail("Ordinary LT input did not dive below the water surface");return;}
        if(third_person||!skeletal_hero.stats().submerged||skeletal_hero.stats().equipment_weapon_visible||skeletal_hero.equipment_combat_ready()){fail("Submerged civilian pose, first-person view or hidden weapon/contact failed");return;}
        // These values come from the completed rendered camera's actual water
        // query, not the logical capsule alone or an injected camera/pose.
        if(water_post_depth<.15f||water_post_blend<.75f){if(age>300)fail("The actual first-person render eye never received underwater composition");return;}
        story_swim_qa_render_underwater_checked[run]=true;
        story_swim_qa_hold_eye=cam.y;next(11);return;
    case 11:
        if(!submerged||!cam.dive.diving||third_person||!skeletal_hero.stats().submerged||skeletal_hero.stats().equipment_weapon_visible){fail("Releasing dive controls did not preserve a submerged neutral swim pose");return;}
        // Releasing LT retains the already requested depth; permit the real
        // damped controller to finish approaching it before measuring drift.
        if(age>=45&&!story_swim_qa_hold_samples[run])story_swim_qa_hold_eye=cam.y;
        if(age>=45){const double drift=std::abs(cam.y-story_swim_qa_hold_eye);story_swim_qa_max_hold_drift[run]=std::max(story_swim_qa_max_hold_drift[run],drift);++story_swim_qa_hold_samples[run];if(drift>.035){fail("Neutral submerged depth drifted after the released-control settling window");return;}}
        if(age<90)return;
        story_swim_qa_hold_checked[run]=story_swim_qa_hold_samples[run]>=45;
        if(!story_swim_qa_hold_checked[run]){fail("Neutral submerged hold did not receive its full observation window");return;}
        story_swim_qa_dove[run]=true;capture("underwater-first");next(6);return;
    case 6:
        if(age<5)return;controller_test_pad.press(SDL_CONTROLLER_BUTTON_RIGHTSTICK);++opening_prologue_qa_events;
        controller_test_pad.axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);story_swim_qa_phase=7;story_swim_qa_action=now;return;
    case 7:
        if(age<5)return;
        if(submerged||cam.dive.diving){if(age>400)fail("Ordinary RT input did not return the swimmer to the surface");return;}
        if(!third_person||!cam.swimming||!water||cam.grounded||std::abs(cam.y-world_swimming::eye_target(surface,support))>.045){if(age>400)fail("Swim camera did not return to settled third-person surface buoyancy");return;}
        capture(story_swim_qa_dove[run]?"resurfaced-third":"surface-return-third");next(8);return;
    case 8:
        if(age<5)return;story_swim_qa_node=0;next(9);return;
    case 9:
        if(!walk(story_swim_qa_back))return;next(10);return;
    case 10:
        if(age<45)return;
        if(cam.swimming||!cam.grounded||!third_person||!skeletal_hero_active||skeletal_hero.stats().swimming||story_appearance_armored||std::abs(cam.y-(support+1.75))>.035){fail("Physical shore exit did not restore the same plain hero, supported walking height and view");return;}
        story_swim_qa_done[run]=true;opening_prologue_qa_pass(loaned?"loaned_plain_hero_surface_swim_and_shore_walk":"preloan_plain_hero_surface_swim_and_shore_walk");capture("shore-third");neutral();return;
    default:fail("Invalid story swim replay phase");return;
    }
};
auto story_swim_qa_report=[&](FILE *file){
    std::fprintf(file,"  \"story_swim_qa\": {\"enabled\": %s, \"passed\": %s, \"require_deep_dive\": %s, \"preloan_complete\": %s, \"loaned_complete\": %s, \"preloan_dive_checked\": %s, \"loaned_dive_checked\": %s, \"surface_float_checked\": [%s, %s], \"surface_float_samples\": [%u, %u], \"maximum_surface_eye_error_m\": [%.6f, %.6f], \"neutral_depth_hold_checked\": [%s, %s], \"neutral_depth_hold_samples\": [%u, %u], \"maximum_neutral_depth_drift_m\": [%.6f, %.6f], \"render_eye_underwater_checked\": [%s, %s], \"phase\": %u, \"node\": %u, \"walked_m\": [%.6f, %.6f], \"steps\": [%u, %u], \"qa_pose_story_health_writes\": 0},\n",story_swim_qa_enabled?"true":"false",story_swim_qa_enabled&&story_swim_qa_done[0]&&story_swim_qa_done[1]?"true":"false",story_swim_qa_require_dive?"true":"false",story_swim_qa_done[0]?"true":"false",story_swim_qa_done[1]?"true":"false",story_swim_qa_dove[0]?"true":"false",story_swim_qa_dove[1]?"true":"false",story_swim_qa_float_checked[0]?"true":"false",story_swim_qa_float_checked[1]?"true":"false",story_swim_qa_float_samples[0],story_swim_qa_float_samples[1],story_swim_qa_max_float_error[0],story_swim_qa_max_float_error[1],story_swim_qa_hold_checked[0]?"true":"false",story_swim_qa_hold_checked[1]?"true":"false",story_swim_qa_hold_samples[0],story_swim_qa_hold_samples[1],story_swim_qa_max_hold_drift[0],story_swim_qa_max_hold_drift[1],story_swim_qa_render_underwater_checked[0]?"true":"false",story_swim_qa_render_underwater_checked[1]?"true":"false",story_swim_qa_phase,story_swim_qa_node,story_swim_qa_walked[0],story_swim_qa_walked[1],story_swim_qa_steps[0],story_swim_qa_steps[1]);
};

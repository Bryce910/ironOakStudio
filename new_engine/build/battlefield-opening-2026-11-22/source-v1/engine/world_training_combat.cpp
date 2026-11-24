#include "engine/world_training_combat.h"
#include "engine/world_melee_contact.h"
#include "engine/world_combat_capacity.h"
#include <algorithm>
#include <cmath>
const char *world_training_direction_name(TrainingDirection d){
    switch(d){case TrainingDirection::Left:return "LEFT";case TrainingDirection::Right:return "RIGHT";case TrainingDirection::High:return "HIGH";case TrainingDirection::Low:return "LOW";case TrainingDirection::Thrust:return "THRUST";}return "HIGH";
}
bool WorldTrainingCombat::engaged()const{return state==TrainingState::Countdown||state==TrainingState::Fighting||state==TrainingState::RoundBreak;}
bool WorldTrainingCombat::near(double x,double y,double z,bool flying)const{return !flying&&std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z)&&std::hypot(x-instructor_x,z-instructor_z)<=3.2&&std::abs(y-25.75)<1.;}
void WorldTrainingCombat::reset_session(){
    const auto equipped=weapon;const auto query=contact_query;void *context=contact_query_context;const auto level=difficulty;const auto high_windup=sword_high_windup_min_seconds;auto *sink=contact_events;
    *this=WorldTrainingCombat{};weapon=equipped;contact_query=query;contact_query_context=context;difficulty=level;sword_high_windup_min_seconds=high_windup;contact_events=sink;
}
void WorldTrainingCombat::message(const char*t,double duration){feedback=t;feedback_time=duration;}
unsigned WorldTrainingCombat::random(unsigned n){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng%n;}
bool WorldTrainingCombat::start(double x,double y,double z,bool flying){
    if(engaged()||!near(x,y,z,flying))return false;
    reset_session();state=TrainingState::Countdown;phase_time=3.;message("Three rounds. Read the guard, vary your angle, watch your stamina.",3);return true;
}
void WorldTrainingCombat::clear_buffer(){pending_attack=false;pending_time=0;pending_attack_serial=0;}
void WorldTrainingCombat::leave(){clear_buffer();if(engaged()){state=TrainingState::Cancelled;guard=false;message("Training ended safely. Return to the master to retry.",4);}}
bool WorldTrainingCombat::start_threat(bool assist,bool reward,bool recover_health){
    if(engaged())return false;
    reset_session();trail_threat=true;story_assist=assist;watch_reward=reward;
    round_health_recovery=recover_health;
    state=TrainingState::Countdown;phase_time=2.5;message("A deserter bars the crossing. Read the windup, parry, then strike.",3);return true;
}
bool WorldTrainingCombat::start_arena(float player_health,float player_stamina,float opponent_health,bool assist){
    if(engaged()||!std::isfinite(player_health)||!std::isfinite(player_stamina)||
       !std::isfinite(opponent_health)||player_health<=0||opponent_health<=0)return false;
    reset_session();arena_mode=trail_threat=true;story_assist=assist;
    health=std::clamp(player_health,0.f,100.f);stamina=std::clamp(player_stamina,0.f,100.f);
    arena_enemy_health=std::clamp(opponent_health,1.f,10000.f);
    enemy_health=enemy_max_health=arena_enemy_health;
    state=TrainingState::Countdown;phase_time=1.;message("Challenger ready. Choose a weapon loadout.",2);return true;
}
bool WorldTrainingCombat::start_shared_arena(bool assist){
    if(!start_arena(100,100,90,assist))return false;
    shared_arena=true;return true;
}
void WorldTrainingCombat::choose(TrainingDirection d){if(unsigned(d)<=unsigned(TrainingDirection::Thrust))selected=d;}
void WorldTrainingCombat::set_guard(bool down){
    if(!down){guard=false;return;}
    // A later explicit defensive intent cancels a previously queued link;
    // holding LT must not unexpectedly release guard into an automatic swing.
    clear_buffer();
    if(state!=TrainingState::Fighting||guard||stamina<3)return;
    guard=true;guard_age=0;stamina-=2;regen_delay=.4;
}
bool WorldTrainingCombat::commit_attack(TrainingDirection direction,bool chain){
    const float cost=(direction==TrainingDirection::Thrust?20.f:16.f)*world_weapon_loadout::profile(weapon).stamina;
    if(state!=TrainingState::Fighting||stamina<cost||dodge_time>0)return false;
    guard=false;stamina-=cost;regen_delay=.45;attack_direction=direction;
    if(hits+misses>0&&last_attack==direction)repeated++;else repeated=0;
    last_attack=direction;direction_mask|=1u<<unsigned(direction);
    // Opponents observe this committed serial in shared_step and react after
    // their difficulty's real delay; input cannot instantaneously move a guard.
    if(chain){++combo_count;++combo_links;}else combo_count=1;
    ++attack_start_serial;attack_commit_clock=clock;clear_buffer();
    player_phase=TrainingPlayerPhase::Windup;player_time=0;
    player_attack_timing=world_combat_timing::attack(weapon,unsigned(direction),chain,sword_high_windup_min_seconds);
    player_duration=player_attack_timing.windup;
    player_resolved=false;player_swoosh=false;return true;
}
bool WorldTrainingCombat::attack(){
    if(weapon==world_weapon_loadout::Weapon::Bow||state!=TrainingState::Fighting||dodge_time>0){++rejected_attacks;return false;}
    if(player_phase==TrainingPlayerPhase::Ready){
        if(commit_attack(selected,false))return true;
        ++rejected_attacks;message("Out of breath - make space and recover");return false;
    }
    if((resolved_attack_serial==attack_start_serial&&resolved_attack_outcome==TrainingAttackOutcome::EnemyBlock)||combo_count>=maximum_combo_swings||pending_attack){++rejected_attacks;return false;}
    // One fresh edge owns exactly the next link of this committed serial.
    // Preserve it until that reachable link, without allowing repeated input
    // to extend its life or carrying it across guard/modal/damage cancellation.
    const auto &timing=committed_timing();double until_link=0;
    if(player_phase==TrainingPlayerPhase::Windup)until_link=std::max(0.,player_duration-player_time)+timing.strike+timing.link;
    else if(player_phase==TrainingPlayerPhase::Strike)until_link=std::max(0.,player_duration-player_time)+timing.link;
    else until_link=std::max(0.,timing.link-player_time);
    pending_attack=true;pending_direction=selected;pending_attack_serial=attack_start_serial;
    // A heavy axe can need 860ms to reach its committed link. The former
    // 800ms cap accepted an early second press, then expired it before that
    // link. Keep the actual remaining interval plus the same 50ms margin;
    // the finite cap still prevents stale input surviving a malformed clock.
    pending_time=std::max(attack_buffer_seconds,std::min(1.5,until_link+.05));++buffered_attacks;return true;
}
bool WorldTrainingCombat::dodge(){
    if(state!=TrainingState::Fighting||dodge_cooldown>0||stamina<23||player_phase!=TrainingPlayerPhase::Ready)return false;
    guard=false;stamina-=23;regen_delay=.75;dodge_time=.26;dodge_cooldown=1.0;dodges++;message("Quick step",.35);return true;
}
void WorldTrainingCombat::next_enemy(TrainingEnemyPhase p,double duration){enemy_phase=p;enemy_time=0;enemy_duration=duration;enemy_resolved=false;enemy_swoosh=false;}
void WorldTrainingCombat::begin_round(){
    enemy_decapitated=false;
    enemy_severed_parts=0;enemy_weapon_hand=TrainingWeaponHand::Right;
    enemy_seen_attack_serial=attack_start_serial;enemy_guard_reaction_pending=false;enemy_guard_reaction_at=enemy_guard_tracking_until=0;enemy_guard_reactions=0;
    enemy_max_health=arena_mode?arena_enemy_health:trail_threat?90.f:48.f+round*14.f;enemy_health=enemy_max_health;
    enemy_guard=TrainingDirection((round+2)%5);guard=false;guard_age=100;
    if(!arena_mode){
        stamina=std::min(100.f,stamina+35);
        if(round_health_recovery)health=std::min(100.f,health+15);
    }
    player_phase=TrainingPlayerPhase::Ready;player_time=0;state=TrainingState::Fighting;
    next_enemy(TrainingEnemyPhase::Measure,.65);message("Read the knight's guard. Attack another angle.",1.5);
}
namespace {
TrainingOpponent legacy_opponent(const WorldTrainingCombat &c){
    TrainingOpponent o;o.health=c.enemy_health;o.max_health=c.enemy_max_health;
    o.direction=c.enemy_direction;o.guard=c.enemy_guard;o.phase=c.enemy_phase;
    o.time=c.enemy_time;o.duration=c.enemy_duration;o.rng=c.rng;
    o.resolved=c.enemy_resolved;o.swoosh=c.enemy_swoosh;o.decapitated=c.enemy_decapitated;
    o.severed_parts=c.enemy_severed_parts;o.weapon_hand=c.enemy_weapon_hand;
    o.seen_attack_serial=c.enemy_seen_attack_serial;o.pending_guard=c.enemy_pending_guard;o.guard_reaction_at=c.enemy_guard_reaction_at;o.guard_tracking_until=c.enemy_guard_tracking_until;o.guard_reaction_pending=c.enemy_guard_reaction_pending;o.guard_reactions=c.enemy_guard_reactions;
    o.hits_received=c.hits;o.hits_dealt=c.enemy_hits;o.swings=c.enemy_swings;
    o.parries_received=c.parries;o.blocks_received=c.blocks;o.blocks_made=c.guarded;
    o.blocked_pose_slot=c.enemy_blocked_pose_slot;o.blocked_pose_weight=c.enemy_blocked_pose_weight;
    o.attack_windup_seconds=c.enemy_attack_windup_seconds;o.attack_start_serial=c.enemy_attack_start_serial;o.blocked_attack_serial=c.enemy_blocked_attack_serial;o.blocked_attack_phase=c.enemy_blocked_attack_phase;o.blocked_attack_time=c.enemy_blocked_attack_time;
    return o;
}
void apply_legacy(WorldTrainingCombat &c,const TrainingOpponent &o){
    c.enemy_health=o.health;c.enemy_max_health=o.max_health;c.enemy_direction=o.direction;
    c.enemy_guard=o.guard;c.enemy_phase=o.phase;c.enemy_time=o.time;c.enemy_duration=o.duration;
    c.rng=o.rng;c.enemy_resolved=o.resolved;c.enemy_swoosh=o.swoosh;c.enemy_decapitated=o.decapitated;
    c.enemy_severed_parts=o.severed_parts;c.enemy_weapon_hand=o.weapon_hand;
    c.enemy_blocked_pose_slot=o.blocked_pose_slot;c.enemy_blocked_pose_weight=o.blocked_pose_weight;
    c.enemy_attack_windup_seconds=o.attack_windup_seconds;c.enemy_attack_start_serial=o.attack_start_serial;c.enemy_blocked_attack_serial=o.blocked_attack_serial;c.enemy_blocked_attack_phase=o.blocked_attack_phase;c.enemy_blocked_attack_time=o.blocked_attack_time;
    c.enemy_seen_attack_serial=o.seen_attack_serial;c.enemy_pending_guard=o.pending_guard;c.enemy_guard_reaction_at=o.guard_reaction_at;c.enemy_guard_tracking_until=o.guard_tracking_until;c.enemy_guard_reaction_pending=o.guard_reaction_pending;c.enemy_guard_reactions=o.guard_reactions;
}
void next_opponent(TrainingOpponent &o,TrainingEnemyPhase p,double duration){
    o.phase=p;o.time=0;o.duration=duration;o.resolved=o.swoosh=false;
    if(p==TrainingEnemyPhase::Windup)o.attack_windup_seconds=duration;
}
unsigned opponent_random(TrainingOpponent &o,unsigned n){o.rng^=o.rng<<13;o.rng^=o.rng>>17;o.rng^=o.rng<<5;return o.rng%n;}
bool valid_hit(const TrainingAnatomicalHit &h){
    if(!h.valid||!std::isfinite(h.sweep_t)||h.sweep_t<0||h.sweep_t>1||!std::isfinite(h.attack_phase)||(h.attack_phase!=-1&&(h.attack_phase<0||h.attack_phase>1)))return false;
    for(double value:h.point)if(!std::isfinite(value)||std::abs(value)>1e7)return false;
    return h.weapon_contact?h.part==TrainingBodyPart::None:world_combat_anatomy::valid(h.part);
}
const char *sever_feedback(TrainingBodyPart p,TrainingWeaponHand hand){
    switch(p){
    case TrainingBodyPart::Head:return "HEAD SEVERED";
    case TrainingBodyPart::Torso:return "TORSO CLEAVED";
    case TrainingBodyPart::LeftLeg:return "LEFT LEG SEVERED";
    case TrainingBodyPart::RightLeg:return "RIGHT LEG SEVERED";
    case TrainingBodyPart::LeftArm:return hand==TrainingWeaponHand::None?"LEFT ARM SEVERED - DISARMED":"LEFT ARM SEVERED - FIGHTING RIGHT-HANDED";
    case TrainingBodyPart::RightArm:return hand==TrainingWeaponHand::None?"RIGHT ARM SEVERED - DISARMED":"RIGHT ARM SEVERED - FIGHTING LEFT-HANDED";
    case TrainingBodyPart::LeftHand:return hand==TrainingWeaponHand::None?"LEFT HAND SEVERED - DISARMED":"LEFT HAND SEVERED - FIGHTING RIGHT-HANDED";
    case TrainingBodyPart::RightHand:return hand==TrainingWeaponHand::None?"RIGHT HAND SEVERED - DISARMED":"RIGHT HAND SEVERED - FIGHTING LEFT-HANDED";
    case TrainingBodyPart::None:break;
    }return "BODY HIT";
}
const char *body_feedback(TrainingBodyPart p){
    switch(p){case TrainingBodyPart::Head:return "HEAD HIT";case TrainingBodyPart::Torso:return "TORSO HIT";
    case TrainingBodyPart::LeftArm:return "LEFT ARM HIT";case TrainingBodyPart::RightArm:return "RIGHT ARM HIT";
    case TrainingBodyPart::LeftHand:return "LEFT HAND HIT";case TrainingBodyPart::RightHand:return "RIGHT HAND HIT";
    case TrainingBodyPart::LeftLeg:return "LEFT LEG HIT";case TrainingBodyPart::RightLeg:return "RIGHT LEG HIT";case TrainingBodyPart::None:break;}return "BODY HIT";
}
}
void WorldTrainingCombat::event(TrainingEvents *events,unsigned id,TrainingContactKind kind,TrainingDirection direction,float damage,bool finisher,const TrainingOpponent *opponent){
    TrainingContactEvent value{++contact_serial,attack_start_serial,id,kind,direction,damage,clock};value.finisher=finisher;
    if(opponent){value.severed_parts=opponent->severed_parts;value.weapon_hand=opponent->weapon_hand;}
    if(!events)return;
    if(events->count<events->capacity)events->values[events->count++]=value;else ++events->overflow;
}
void WorldTrainingCombat::defeated(TrainingOpponent &o,unsigned id,TrainingEvents *events){
    if(o.health<=0&&!o.defeat_announced){o.defeat_announced=true;const unsigned before=events?events->count:0;event(events,id,TrainingContactKind::EnemyDefeated,o.reaction_direction);if(events&&events->count>before){events->values[before].severed_parts=o.severed_parts;events->values[before].weapon_hand=o.weapon_hand;}}
}
WorldTrainingCombat WorldTrainingCombat::opponent_view(const TrainingOpponent &o)const{
    auto view=*this;apply_legacy(view,o);
    view.hits=o.hits_received;view.enemy_hits=o.hits_dealt;view.enemy_swings=o.swings;
    view.parries=o.parries_received;view.blocks=o.blocks_received;view.guarded=o.blocks_made;
    view.attack_direction=o.reaction_direction;
    view.attack_start_serial=o.attack_start_serial;
    view.resolved_attack_pose_slot=o.blocked_pose_slot;view.resolved_attack_pose_weight=o.blocked_pose_weight;
    view.resolved_attack_serial=o.blocked_attack_serial;view.resolved_attack_phase=o.blocked_attack_phase;view.resolved_attack_time=o.blocked_attack_time;
    view.resolved_attack_outcome=o.blocked_attack_serial?TrainingAttackOutcome::EnemyBlock:TrainingAttackOutcome::None;
    if(o.health<=0)view.state=TrainingState::Won;
    return view;
}
void WorldTrainingCombat::finish_shared_arena(bool won){
    if(!shared_arena)return;
    state=won?TrainingState::Won:TrainingState::Lost;guard=false;clear_buffer();
    player_phase=TrainingPlayerPhase::Ready;combo_count=0;
    message(won?"Arena complete - all challengers defeated":"You yield. Prepare another practice run.",8);
}
bool WorldTrainingCombat::damage_opponent(TrainingOpponent &o,unsigned id,float damage,TrainingDirection direction,TrainingEvents &events){
    if(!shared_arena||state!=TrainingState::Fighting||id>=world_combat_capacity::opponents||o.health<=0||
       !std::isfinite(damage)||damage<=0||damage>10000||unsigned(direction)>4)return false;
    const float actual=std::min(o.health,damage);o.health-=actual;o.reaction_direction=direction;
    ++o.hits_received;++hits;event(&events,id,TrainingContactKind::PlayerBody,direction,actual,o.health<=0,&o);
    defeated(o,id,&events);regen_delay=std::max(regen_delay,.35);
    if(impact_pause_enabled)impact_pause=std::max(impact_pause,.025);
    message("Arrow - BODY HIT",.7);return true;
}
void WorldTrainingCombat::update(double dt,double x,double z,bool paused){
    // Shared arenas must use update_shared exactly once; a legacy call cannot
    // accidentally duplicate their stamina regeneration or advance their clock.
    if(shared_arena)return;
    if(!std::isfinite(dt)||dt<0||!std::isfinite(x)||!std::isfinite(z))return;
    if(paused){guard=false;clear_buffer();return;}
    if(engaged()&&!arena_mode&&std::hypot(x-enemy_x,z-enemy_z)>9.){leave();return;}
    double remaining=std::min(dt,.25);const double distance=std::hypot(x-enemy_x,z-enemy_z);
    while(remaining>1e-9){const double h=std::min(remaining,1./240.);if(impact_pause>0){impact_pause=std::max(0.,impact_pause-h);}else step(h,distance);remaining-=h;}
}
bool WorldTrainingCombat::update_shared(double dt,TrainingTarget *targets,std::size_t count,TrainingEvents &events,bool paused){
    if(!shared_arena||!std::isfinite(dt)||dt<0||count>world_combat_capacity::opponents||(count&&!targets))return false;
    for(std::size_t i=0;i<count;++i){
        const auto &t=targets[i];if(!t.opponent||t.id>=world_combat_capacity::opponents||!std::isfinite(t.distance)||t.distance<0||
            !std::isfinite(t.opponent->health)||!std::isfinite(t.opponent->max_health)||t.opponent->health<0||
            t.opponent->max_health<=0||t.opponent->health>t.opponent->max_health||
            !std::isfinite(t.opponent->attack_windup_seconds)||t.opponent->attack_windup_seconds<=0||t.opponent->attack_windup_seconds>10||
            !std::isfinite(t.opponent->time)||!std::isfinite(t.opponent->duration)||t.opponent->time<0||
            t.opponent->duration<=0||unsigned(t.opponent->phase)>4||unsigned(t.opponent->direction)>4||unsigned(t.opponent->guard)>4||
            (t.opponent->severed_parts&~std::uint16_t(255))||unsigned(t.opponent->weapon_hand)>2||
            !std::isfinite(t.opponent->guard_reaction_at)||!std::isfinite(t.opponent->guard_tracking_until)||
            t.opponent->guard_reaction_at<0||t.opponent->guard_tracking_until<0)return false;
        if(t.spatial&&(!std::isfinite(t.local_x)||!std::isfinite(t.local_z)||
           std::abs(std::hypot(t.local_x,t.local_z)-t.distance)>.01))return false;
        for(std::size_t j=0;j<i;++j)if(targets[j].id==t.id||targets[j].opponent==t.opponent)return false;
    }
    if(paused){guard=false;clear_buffer();return true;}
    double remaining=std::min(dt,.25);
    while(remaining>1e-9){const double h=std::min(remaining,1./240.);if(impact_pause>0)impact_pause=std::max(0.,impact_pause-h);else shared_step(h,targets,count,&events);remaining-=h;}
    return true;
}
void WorldTrainingCombat::step(double dt,double distance){
    auto o=legacy_opponent(*this);TrainingTarget target{&o,0,distance};
    shared_step(dt,&target,1,contact_events);apply_legacy(*this,o);
    if(state!=TrainingState::Fighting||enemy_health>0)return;
    guard=false;clear_buffer();player_phase=TrainingPlayerPhase::Ready;combo_count=0;
    if(trail_threat||round==2){state=TrainingState::Won;message(arena_mode?"Opponent yields. Prepare for the next challenger.":trail_threat?"The crossing is clear. Find Warden Rowan at the fire.":"Training complete - three knights defeated",8);}
    else{++round;state=TrainingState::RoundBreak;phase_time=3.;message("Round won. Catch your breath - next knight in 3 seconds",3);}
}
void WorldTrainingCombat::shared_step(double dt,TrainingTarget *targets,std::size_t count,TrainingEvents *events){
    clock+=dt;feedback_time=std::max(0.,feedback_time-dt);
    if(!engaged())return;
    if(state==TrainingState::Countdown||state==TrainingState::RoundBreak){
        phase_time-=dt;
        if(phase_time<=0){
            if(shared_arena){state=TrainingState::Fighting;player_phase=TrainingPlayerPhase::Ready;message("Read the guards. Make space between attacks.",1.5);}
            else{
                begin_round();
                if(count){auto &o=*targets[0].opponent;o=legacy_opponent(*this);}
            }
        }
        return;
    }
    guard_age+=dt;regen_delay=std::max(0.,regen_delay-dt);dodge_time=std::max(0.,dodge_time-dt);dodge_cooldown=std::max(0.,dodge_cooldown-dt);
    if(pending_attack){
        if(pending_attack_serial!=attack_start_serial||player_phase==TrainingPlayerPhase::Ready)clear_buffer();
        else{pending_time-=dt;if(pending_time<=0){clear_buffer();++expired_buffers;}}
    }
    if(regen_delay<=0&&player_phase==TrainingPlayerPhase::Ready)stamina=std::min(100.f,stamina+float(dt*(guard?guard_stamina_per_second:ready_stamina_per_second+(watch_reward?7:0))*town_recovery));
    for(std::size_t i=0;i<count;++i){
        auto &o=*targets[i].opponent;
        if(o.seen_attack_serial!=attack_start_serial&&(!arena_mode||(targets[i].advance_ai&&targets[i].in_player_arc&&targets[i].distance<=world_weapon_loadout::profile(weapon).reach+.8))){
            o.seen_attack_serial=attack_start_serial;o.pending_guard=attack_direction;
            o.guard_reaction_at=clock+(arena_mode?world_combat_difficulty::arena_reaction_delay_s(difficulty,repeated>=1):world_combat_difficulty::reaction_delay_s(difficulty,repeated>=1));o.guard_reaction_pending=true;
        }
        if(o.guard_reaction_pending&&o.phase==TrainingEnemyPhase::Measure&&world_combat_anatomy::can_fight(o.severed_parts)&&clock>=o.guard_reaction_at&&clock>=o.guard_tracking_until){
            o.guard=o.pending_guard;o.guard_reaction_pending=false;++o.guard_reactions;
            o.guard_tracking_until=clock+(arena_mode?world_combat_difficulty::arena_guard_profile(difficulty):world_combat_difficulty::profile(difficulty)).guard_tracking_interval_s;
        }
    }
    auto resolve_player=[&](double phase_begin,double phase_end,double interval,bool finalize){
            // One committed sword strike resolves against its closest actual
            // reachable defender. No locked target, rear hit or eight-way mint.
            TrainingTarget *chosen=nullptr;TrainingAnatomicalHit chosen_hit;
            const double windup_seconds=committed_timing().windup;
            for(std::size_t i=0;i<count;++i){auto &t=targets[i];if(t.opponent->health<=0||!t.in_player_arc)continue;
                const auto &o=*t.opponent;
                const bool opening=o.phase==TrainingEnemyPhase::Recovery||o.phase==TrainingEnemyPhase::Stagger;
                const bool weapon_guard=!opening&&world_combat_anatomy::can_fight(o.severed_parts)&&o.guard==attack_direction;
                TrainingAnatomicalHit hit;
                if(contact_query){
                    if(t.distance>world_weapon_loadout::profile(weapon).reach+.8)continue;
                    TrainingAnatomicalQuery query;query.opponent_id=t.id;query.weapon=weapon;query.direction=attack_direction;query.attack_serial=attack_start_serial;
                    query.local_x=t.local_x;query.local_z=t.local_z;query.distance=t.distance;query.spatial=t.spatial;query.weapon_guard=weapon_guard;
                    query.phase_begin=phase_begin;query.phase_end=phase_end;query.interval_seconds=interval;
                    query.contact_clock=clock;query.windup_seconds=windup_seconds;query.attack_commit_clock=attack_commit_clock;
                    query.strike_seconds=committed_timing().strike;query.recovery_seconds=committed_timing().recovery;query.combo_link_seconds=committed_timing().link;query.defender_phase=o.phase;query.defender_direction=o.direction;query.defender_guard=o.guard;
                    query.defender_time=o.time;query.defender_duration=o.duration;query.severed_parts=o.severed_parts;
                    query.defender_clash_serial=o.blocked_attack_serial;query.defender_clash_phase=o.blocked_attack_phase;query.defender_clash_time=o.blocked_attack_time;
                    query.defender_clash_pose_slot=o.blocked_pose_slot;query.defender_clash_pose_weight=o.blocked_pose_weight;
                    if(!contact_query(query,hit,contact_query_context)||!valid_hit(hit)||(!hit.weapon_contact&&world_combat_anatomy::removed(o.severed_parts,hit.part)))continue;
                    if(!chosen||hit.sweep_t<chosen_hit.sweep_t-1e-9||(std::abs(hit.sweep_t-chosen_hit.sweep_t)<=1e-9&&(t.distance<chosen->distance-1e-9||(std::abs(t.distance-chosen->distance)<=1e-9&&t.id<chosen->id)))){chosen=&t;chosen_hit=hit;}
                    continue;
                }
                if(t.distance>world_weapon_loadout::profile(weapon).reach||t.distance<1.)continue;
                if(world_melee_contact::calibrated(weapon)){
                    const auto &o=*t.opponent;
                    const bool opening=o.phase==TrainingEnemyPhase::Recovery||o.phase==TrainingEnemyPhase::Stagger;
                    const bool weapon_guard=!opening&&world_combat_anatomy::can_fight(o.severed_parts)&&o.guard==attack_direction;
                    if(!world_melee_contact::intersects(weapon,unsigned(attack_direction),t.spatial?t.local_x:0.,t.spatial?t.local_z:t.distance,weapon_guard))continue;
                }
                if(!chosen||t.distance<chosen->distance-1e-9||(std::abs(t.distance-chosen->distance)<=1e-9&&t.id<chosen->id))chosen=&t;
            }
            if(!chosen&&!finalize)return false;
            // The latch and optional event describe this exact resolved swing,
            // independently of incoming damage/guard counters later this step.
            resolved_attack_serial=attack_start_serial;
            resolved_attack_direction=attack_direction;
            resolved_attack_point={};resolved_attack_anatomical=false;
            resolved_attack_opponent=chosen?chosen->id:world_combat_capacity::npcs;
            resolved_attack_phase=contact_query&&chosen?phase_begin+(phase_end-phase_begin)*chosen_hit.sweep_t:.36;
            resolved_attack_time=clock;resolved_attack_damage=0;
            if(!chosen){
                resolved_attack_outcome=TrainingAttackOutcome::Miss;
                ++misses;event(events,world_combat_capacity::npcs,TrainingContactKind::PlayerMiss,attack_direction);
                message("MISSED - adjust range or aim");
            }
            else{
                auto &o=*chosen->opponent;
                const bool opening=o.phase==TrainingEnemyPhase::Recovery||o.phase==TrainingEnemyPhase::Stagger;
                const bool blocked=contact_query?chosen_hit.weapon_contact:!opening&&world_combat_anatomy::can_fight(o.severed_parts)&&o.guard==attack_direction;
                resolved_attack_outcome=blocked?TrainingAttackOutcome::EnemyBlock:TrainingAttackOutcome::Body;
                if(contact_query){resolved_attack_point={chosen_hit.point[0],chosen_hit.point[1],chosen_hit.point[2]};resolved_attack_anatomical=true;}
                if(blocked){
                    clear_buffer();combo_count=0;player_phase=TrainingPlayerPhase::Recovery;player_time=0;player_duration=std::max(.32,committed_timing().recovery);
                    if(contact_query&&(o.phase==TrainingEnemyPhase::Windup||o.phase==TrainingEnemyPhase::Strike)){
                        // Two moving blades collide: neither can continue the
                        // stopped strike and deal a later phantom body hit.
                        o.blocked_attack_serial=o.attack_start_serial;o.blocked_attack_time=clock;
                        o.blocked_attack_phase=chosen_hit.defender_attack_phase>=0?chosen_hit.defender_attack_phase:.36;
                        o.blocked_pose_slot=chosen_hit.defender_pose_slot;o.blocked_pose_weight=chosen_hit.defender_pose_weight;
                        next_opponent(o,TrainingEnemyPhase::Recovery,.34);o.resolved=true;
                    }
                    ++misses;++guarded;++o.blocks_made;if(impact_pause_enabled)impact_pause=std::max(impact_pause,.025);const unsigned before=events?events->count:0;event(events,chosen->id,TrainingContactKind::EnemyBlock,attack_direction,0,false,&o);if(events&&events->count>before&&contact_query){auto &e=events->values[before];e.anatomical=true;std::copy_n(chosen_hit.point,3,e.point);}message("STEEL CLASH - change your attack angle",.8);}
                else{
                    const bool riposte=o.phase==TrainingEnemyPhase::Stagger;
                    float damage=std::min(o.health,(riposte?25.f:15.f)*(story_assist?1.25f:1.f)*town_damage*world_weapon_loadout::profile(weapon).damage);
                    const bool sharp=contact_query&&chosen_hit.cutting_edge&&world_weapon_loadout::profile(weapon).edged&&(arena_mode||trail_threat);
                    const bool sever=sharp&&attack_direction!=TrainingDirection::Thrust;
                    if(sever){
                        o.severed_parts|=world_combat_anatomy::bit(chosen_hit.part);o.weapon_hand=world_combat_anatomy::surviving_hand(o.severed_parts);
                        damage=world_combat_anatomy::surviving_limb(chosen_hit.part)?std::min(damage,std::max(0.f,o.health-1.f)):o.health;
                        if(world_combat_anatomy::surviving_limb(chosen_hit.part)){next_opponent(o,TrainingEnemyPhase::Stagger,.35);o.guard_reaction_pending=false;}
                    }else if(sharp&&attack_direction==TrainingDirection::Thrust&&(chosen_hit.part==TrainingBodyPart::Torso||chosen_hit.part==TrainingBodyPart::Head))damage=o.health;
                    o.health-=damage;resolved_attack_damage=damage;o.reaction_direction=attack_direction;++hits;++o.hits_received;
                    // A landed blunt hit must interrupt physically before a
                    // full-body flinch can own the displayed pose. Never let a
                    // cosmetic reaction hide an AI strike still dealing damage,
                    // and do not refresh an already staggered opponent.
                    if(contact_query&&!world_weapon_loadout::profile(weapon).edged&&damage>0&&o.health>0&&o.phase!=TrainingEnemyPhase::Stagger){
                        next_opponent(o,TrainingEnemyPhase::Stagger,.40);o.guard_reaction_pending=false;
                    }
                    // A direction label alone never names an anatomical zone.
                    o.decapitated=sever&&chosen_hit.part==TrainingBodyPart::Head;
                    if(impact_pause_enabled)impact_pause=std::max(impact_pause,sever?.075:riposte?.045:.032);
                    const unsigned before=events?events->count:0;
                    event(events,chosen->id,TrainingContactKind::PlayerBody,attack_direction,damage,o.health<=0,&o);
                    if(events&&events->count>before){auto &e=events->values[before];e.decapitated=o.decapitated;e.severed_parts=o.severed_parts;e.weapon_hand=o.weapon_hand;if(contact_query){e.anatomical=true;e.body_part=chosen_hit.part;e.severed_part=sever?chosen_hit.part:TrainingBodyPart::None;std::copy_n(chosen_hit.point,3,e.point);}}
                    defeated(o,chosen->id,events);
                    const bool lethal_thrust=sharp&&attack_direction==TrainingDirection::Thrust&&o.health<=0;
                    message(sever?sever_feedback(chosen_hit.part,o.weapon_hand):lethal_thrust?(chosen_hit.part==TrainingBodyPart::Head?"LETHAL HEAD THRUST":"LETHAL TORSO THRUST"):contact_query?body_feedback(chosen_hit.part):riposte?"RIPOSTE - BODY HIT":"BODY HIT",sever?1.4:.7);
                }
            }
            player_resolved=true;return true;
    };
    auto phase_of=[&](){
        if(player_phase==TrainingPlayerPhase::Windup){if(player_time<=player_duration)return .36*std::clamp(player_time/std::max(.001,player_duration),0.,1.);return .36+.24*std::clamp((player_time-player_duration)/std::max(.001,committed_timing().strike),0.,1.);}
        if(player_phase==TrainingPlayerPhase::Strike)return .36+.24*std::clamp(player_time/std::max(.001,player_duration),0.,1.);
        if(player_phase==TrainingPlayerPhase::Recovery)return .60+.40*std::clamp(player_time/std::max(.001,player_duration),0.,1.);
        return 0.;
    };
    if(player_phase!=TrainingPlayerPhase::Ready){
        const double before=phase_of();
        player_time+=dt;
        if(player_phase==TrainingPlayerPhase::Windup&&!player_swoosh&&player_time>=std::max(0.,player_duration-.15)){
            player_swoosh=true;++player_swings;event(events,world_combat_capacity::npcs,TrainingContactKind::PlayerSwing,attack_direction);
        }
        if(contact_query&&!player_resolved&&(player_phase==TrainingPlayerPhase::Windup||player_phase==TrainingPlayerPhase::Strike)){
            const double after=phase_of(),lo=std::max(.26,before),hi=std::min(.60,after);
            if(hi>=lo&&after>=.26)resolve_player(lo,hi,dt*(hi-lo)/std::max(1e-12,after-before),after>=.60);
        }
        if(player_phase==TrainingPlayerPhase::Windup&&player_time>=player_duration){
            player_phase=TrainingPlayerPhase::Strike;player_time-=player_duration;player_duration=committed_timing().strike;
            if(!contact_query)resolve_player(.26,.36,0,true);

        }else if(player_phase==TrainingPlayerPhase::Strike&&player_time>=player_duration){player_phase=TrainingPlayerPhase::Recovery;player_time-=player_duration;player_duration=committed_timing().recovery;}
        else if(player_phase==TrainingPlayerPhase::Recovery){
            if(pending_attack&&combo_count<maximum_combo_swings&&player_time>=committed_timing().link){
                const auto direction=pending_direction;
                if(!commit_attack(direction,true)){clear_buffer();message("Out of breath - combo ends",.8);}
            }
            if(player_phase==TrainingPlayerPhase::Recovery&&player_time>=player_duration){player_phase=TrainingPlayerPhase::Ready;player_time=0;combo_count=0;clear_buffer();}
        }
    }
    auto resolve_incoming=[&](TrainingTarget &t,double begin,double end,double interval){
        auto &o=*t.opponent;
        if(o.resolved||o.health<=0||t.distance>enemy_reach+.8||dodge_time>0)return;
        const bool effective_guard=t.player_can_guard&&guard&&player_phase==TrainingPlayerPhase::Ready&&stamina>=8&&weapon!=world_weapon_loadout::Weapon::Bow;
        TrainingAnatomicalQuery query;query.incoming=true;query.opponent_id=t.id;query.weapon=weapon;query.direction=o.direction;
        query.attack_serial=o.attack_start_serial;query.phase_begin=begin;query.phase_end=end;query.interval_seconds=interval;
        query.contact_clock=clock;query.weapon_guard=effective_guard;query.defender_guard=selected;
        query.windup_seconds=o.attack_windup_seconds;
        query.strike_seconds=o.phase==TrainingEnemyPhase::Strike?o.duration:world_combat_timing::incoming_strike(unsigned(o.direction),o.attack_windup_seconds,round);
        query.local_x=t.local_x;query.local_z=t.local_z;query.distance=t.distance;query.spatial=t.spatial;query.severed_parts=o.severed_parts;
        TrainingAnatomicalHit hit;
        if(!contact_query(query,hit,contact_query_context)||!valid_hit(hit)){
            if(end>=.60)o.resolved=true;
            return;
        }
        const double hit_phase=hit.attack_phase>=0?hit.attack_phase:begin+(end-begin)*hit.sweep_t;
        const auto point_event=[&](TrainingContactKind kind,float damage,bool broken=false){
            const unsigned before=events?events->count:0;event(events,t.id,kind,o.direction,damage,health<=0,&o);
            if(events&&events->count>before){auto &e=events->values[before];e.attack_serial=o.attack_start_serial;e.anatomical=true;e.guard_broken=broken;
                e.body_part=hit.part;std::copy_n(hit.point,3,e.point);}
        };
        if(hit.weapon_contact){
            if(!effective_guard)return;
            if(guard_age>parry_window_seconds&&stamina<world_weapon_loadout::profile(weapon).block_cost){
                // Steel contact is real, but exhaustion cannot magically stop
                // the attacker or deal damage before a later body intersection.
                stamina=0;guard=false;clear_buffer();regen_delay=.5;
                point_event(TrainingContactKind::PlayerBlock,0,true);message("GUARD BROKEN - recover your stance",.8);return;
            }
            if(!o.attack_start_serial)++o.attack_start_serial;
            o.blocked_attack_serial=o.attack_start_serial;o.blocked_attack_phase=hit_phase;o.blocked_attack_time=clock;
            o.blocked_pose_slot=~0u;o.blocked_pose_weight=0;
            o.resolved=true;regen_delay=.4;
            if(guard_age<=parry_window_seconds){
                stamina-=7;++parries;++o.parries_received;
                if(impact_pause_enabled)impact_pause=std::max(impact_pause,.045);
                next_opponent(o,TrainingEnemyPhase::Stagger,parry_stagger_seconds);o.resolved=true;
                point_event(TrainingContactKind::PlayerParry,0);message("STEEL PARRY - RIPOSTE!",parry_stagger_seconds);
            }else{
                stamina-=world_weapon_loadout::profile(weapon).block_cost;++blocks;++o.blocks_received;
                if(impact_pause_enabled)impact_pause=std::max(impact_pause,.030);
                next_opponent(o,TrainingEnemyPhase::Recovery,.34);o.resolved=true;
                point_event(TrainingContactKind::PlayerBlock,0);message("STEEL BLOCK - counter or adjust your guard",.8);
            }
            return;
        }
        o.resolved=true;
        const float damage=std::min(health,(18+round*3.f)*(story_assist?.45f:1.f)*town_resistance*gear_resistance);
        health-=damage;++enemy_hits;++o.hits_dealt;clear_buffer();regen_delay=std::max(regen_delay,.4);
        if(impact_pause_enabled)impact_pause=std::max(impact_pause,.04);
        point_event(TrainingContactKind::EnemyBody,damage);message(guard?"BODY HIT - blade passed your guard":"BODY HIT - intercept the incoming blade",1.);
        if(health<=0){state=TrainingState::Lost;guard=false;clear_buffer();message("Yield. Press R to prepare another bout",8);}
    };
    for(std::size_t i=0;i<count&&state==TrainingState::Fighting;++i){
        auto &t=targets[i];auto &o=*t.opponent;if(o.health<=0){defeated(o,t.id,events);continue;}
        if(!t.advance_ai)continue; // This hostile is currently engaging an ally; the arena owns its AI clock.
        if(!world_combat_anatomy::can_fight(o.severed_parts)){
            // Both weapon hands gone: alive, targetable, but no invisible
            // attacks/blocks. A subsequent real body/head contact can finish.
            next_opponent(o,TrainingEnemyPhase::Measure,.25);o.guard_reaction_pending=false;continue;
        }
        // Approach is not a free hit: perception/guard above remains active,
        // but an entering actor cannot start a root-planted attack early.
        if(!t.can_commit_attack&&o.phase==TrainingEnemyPhase::Measure)continue;
        const double tempo=world_combat_difficulty::profile(difficulty).attack_tempo_scale;
        const double strike_seconds=o.phase==TrainingEnemyPhase::Strike?o.duration:world_combat_timing::incoming_strike(unsigned(o.direction),o.attack_windup_seconds,round);
        const auto enemy_phase=[&](){
            if(o.phase==TrainingEnemyPhase::Windup){if(o.time<=o.duration)return .36*std::clamp(o.time/std::max(.001,o.duration),0.,1.);return .36+.24*std::clamp((o.time-o.duration)/strike_seconds,0.,1.);}
            if(o.phase==TrainingEnemyPhase::Strike)return .36+.24*std::clamp(o.time/std::max(.001,o.duration),0.,1.);
            return 0.;
        };
        const double enemy_before=enemy_phase();
        o.time+=dt;
        if(contact_query&&!o.resolved&&(o.phase==TrainingEnemyPhase::Windup||o.phase==TrainingEnemyPhase::Strike)){
            const double after=enemy_phase(),lo=std::max(.26,enemy_before),hi=std::min(.60,after);
            if(hi>=lo&&after>=.26)resolve_incoming(t,lo,hi,dt*(hi-lo)/std::max(1e-12,after-enemy_before));
        }
        if(o.phase==TrainingEnemyPhase::Windup&&!o.swoosh&&o.time>=o.duration*.68){o.swoosh=true;++o.swings;++enemy_swings;
            const unsigned before=events?events->count:0;event(events,t.id,TrainingContactKind::EnemySwing,o.direction,0,false,&o);
            if(events&&events->count>before)events->values[before].attack_serial=o.attack_start_serial;
        }
        if(o.time<o.duration)continue;
        switch(o.phase){
        case TrainingEnemyPhase::Measure:
            // Only an uncommitted defender may wait for a visible cut. Never
            // interrupt an attack or manufacture a successful block here.
            if(arena_mode&&attack_start_serial&&!player_resolved&&
               (player_phase==TrainingPlayerPhase::Windup||player_phase==TrainingPlayerPhase::Strike)&&
               t.in_player_arc&&t.distance<=world_weapon_loadout::profile(weapon).reach+.8&&
               o.time<o.duration+world_combat_difficulty::arena_defensive_hold_s(difficulty))break;
            if(arena_mode&&t.distance>enemy_reach+.05){next_opponent(o,TrainingEnemyPhase::Measure,.08);break;}
            if(arena_mode){
                // Choose once at commitment. Avoid a rote alternating pattern;
                // an established player guard influences (but never dictates)
                // the choice. No mid-swing retargeting or invisible feint.
                unsigned next=opponent_random(o,5);
                if(next==unsigned(o.direction)&&opponent_random(o,100)<80)next=(next+1+opponent_random(o,4))%5;
                if(guard&&guard_age>.12&&next==unsigned(selected)&&opponent_random(o,100)<70)next=(next+1+opponent_random(o,4))%5;
                o.direction=TrainingDirection(next);
            }else{o.direction=TrainingDirection(opponent_random(o,5));o.guard=TrainingDirection((unsigned(o.direction)+2+round)%5);}
            ++o.attack_start_serial;
            {
                double windup=world_combat_timing::incoming(unsigned(o.direction),difficulty,round).windup;
                if(arena_mode)windup=std::max(.46,windup*(.90+opponent_random(o,29)*.01));
                // next_opponent latches this exact value for source animation,
                // release, recovery and contact prediction for the whole cut.
                next_opponent(o,TrainingEnemyPhase::Windup,windup);
            }break;
        case TrainingEnemyPhase::Windup:
            if(!contact_query){
            next_opponent(o,TrainingEnemyPhase::Strike,strike_seconds);
            if(t.distance>enemy_reach)message("Knight missed - close in and counter!",.9);
            else if(dodge_time>0)message("Evaded - counter now!",.9);
            else if(t.player_can_guard&&guard&&selected==o.direction&&player_phase==TrainingPlayerPhase::Ready&&stamina>=8){
                if(guard_age<=parry_window_seconds){stamina-=7;++parries;++o.parries_received;if(impact_pause_enabled)impact_pause=std::max(impact_pause,.045);next_opponent(o,TrainingEnemyPhase::Stagger,parry_stagger_seconds);event(events,t.id,TrainingContactKind::PlayerParry,o.direction,0,false,&o);message("STEEL PARRY - RIPOSTE!",parry_stagger_seconds);}
                else if(stamina>=world_weapon_loadout::profile(weapon).block_cost){stamina-=world_weapon_loadout::profile(weapon).block_cost;++blocks;++o.blocks_received;if(impact_pause_enabled)impact_pause=std::max(impact_pause,.025);event(events,t.id,TrainingContactKind::PlayerBlock,o.direction,0,false,&o);message("STEEL BLOCK - release guard to recover",.8);}
                else{
                    stamina=0;const float damage=std::min(health,(18+round*3.f)*(story_assist?.45f:1.f)*town_resistance*gear_resistance);health-=damage;++enemy_hits;++o.hits_dealt;clear_buffer();
                    if(impact_pause_enabled)impact_pause=std::max(impact_pause,.04);
                    guard=false;event(events,t.id,TrainingContactKind::EnemyBody,o.direction,damage,health<=0,&o);message("GUARD BROKEN - BODY HIT",1.);
                }
                regen_delay=.4;
            }else{
                const float damage=std::min(health,(18+round*3.f)*(story_assist?.45f:1.f)*town_resistance*gear_resistance);health-=damage;++enemy_hits;++o.hits_dealt;clear_buffer();
                if(impact_pause_enabled)impact_pause=std::max(impact_pause,.04);
                event(events,t.id,TrainingContactKind::EnemyBody,o.direction,damage,health<=0,&o);message(guard?"BODY HIT - wrong guard angle":"BODY HIT - defend the incoming direction",1.);
            }
            if(health<=0){state=TrainingState::Lost;guard=false;clear_buffer();message("Yield. No injury - press E to retry or Esc to leave",8);}
            }else{const bool resolved=o.resolved;const double overflow=std::max(0.,o.time-o.duration);next_opponent(o,TrainingEnemyPhase::Strike,strike_seconds);o.resolved=resolved;o.time=overflow;}
            break;
        case TrainingEnemyPhase::Strike:next_opponent(o,TrainingEnemyPhase::Recovery,world_combat_timing::incoming_recovery(o.attack_windup_seconds,round));break;
        case TrainingEnemyPhase::Recovery:case TrainingEnemyPhase::Stagger:
            next_opponent(o,TrainingEnemyPhase::Measure,(arena_mode?(.20+opponent_random(o,66)*.01):(.35+opponent_random(o,21)*.01))/tempo);
            if(arena_mode&&world_combat_difficulty::arena_remembers_repeated_angle(difficulty)&&repeated>=1&&attack_start_serial){
                // Telegraph a learned guard after the full committed recovery.
                // A fresh angle still schedules its own delayed source change.
                o.pending_guard=last_attack;o.guard_reaction_pending=true;
                o.guard_reaction_at=clock+world_combat_difficulty::arena_reaction_delay_s(difficulty,true);
            }break;
        }
    }
}
float WorldTrainingCombat::enemy_animation()const{
    const float t=float(std::clamp(enemy_time/std::max(.001,enemy_duration),0.,1.));
    if(enemy_phase==TrainingEnemyPhase::Windup)return t*.30f;
    if(enemy_phase==TrainingEnemyPhase::Strike)return .30f+t*.28f;
    if(enemy_phase==TrainingEnemyPhase::Recovery)return .58f+t*.4199f;
    if(enemy_phase==TrainingEnemyPhase::Stagger)return .30f*(1-t*t*(3-2*t));
    return 0;
}
float WorldTrainingCombat::player_animation()const{return float(std::clamp(player_time/std::max(.001,player_duration),0.,1.));}
const char *WorldTrainingCombat::opponent_name()const{return trail_threat?"The oathbreaker":round==0?"Ser Aldric - Recruit":round==1?"Ser Bran - Veteran":"Master Corvin - Champion";}

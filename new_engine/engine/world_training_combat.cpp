#include "engine/world_training_combat.h"
#include <algorithm>
#include <cmath>
const char *world_training_direction_name(TrainingDirection d){
    switch(d){case TrainingDirection::Left:return "LEFT";case TrainingDirection::Right:return "RIGHT";case TrainingDirection::High:return "HIGH";case TrainingDirection::Low:return "LOW";case TrainingDirection::Thrust:return "THRUST";}return "HIGH";
}
bool WorldTrainingCombat::engaged()const{return state==TrainingState::Countdown||state==TrainingState::Fighting||state==TrainingState::RoundBreak;}
bool WorldTrainingCombat::near(double x,double y,double z,bool flying)const{return !flying&&std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z)&&std::hypot(x-instructor_x,z-instructor_z)<=3.2&&std::abs(y-25.75)<1.;}
void WorldTrainingCombat::message(const char*t,double duration){feedback=t;feedback_time=duration;}
unsigned WorldTrainingCombat::random(unsigned n){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng%n;}
bool WorldTrainingCombat::start(double x,double y,double z,bool flying){
    if(engaged()||!near(x,y,z,flying))return false;
    *this=WorldTrainingCombat{};state=TrainingState::Countdown;phase_time=3.;message("Three rounds. Read the guard, vary your angle, watch your stamina.",3);return true;
}
void WorldTrainingCombat::clear_buffer(){pending_attack=false;pending_time=0;}
void WorldTrainingCombat::leave(){clear_buffer();if(engaged()){state=TrainingState::Cancelled;guard=false;message("Training ended safely. Return to the master to retry.",4);}}
bool WorldTrainingCombat::start_threat(bool assist,bool reward){
    if(engaged())return false;
    *this=WorldTrainingCombat{};trail_threat=true;story_assist=assist;watch_reward=reward;
    state=TrainingState::Countdown;phase_time=2.5;message("A deserter bars the crossing. Read the windup, parry, then strike.",3);return true;
}
bool WorldTrainingCombat::start_arena(float player_health,float player_stamina,float opponent_health,bool assist){
    if(engaged()||!std::isfinite(player_health)||!std::isfinite(player_stamina)||
       !std::isfinite(opponent_health)||player_health<=0||opponent_health<=0)return false;
    *this=WorldTrainingCombat{};arena_mode=trail_threat=true;story_assist=assist;
    health=std::clamp(player_health,0.f,100.f);stamina=std::clamp(player_stamina,0.f,100.f);
    arena_enemy_health=std::clamp(opponent_health,1.f,10000.f);
    enemy_health=enemy_max_health=arena_enemy_health;
    state=TrainingState::Countdown;phase_time=1.;message("Challenger ready. Choose sword or bow.",2);return true;
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
    const float cost=direction==TrainingDirection::Thrust?20.f:16.f;
    if(state!=TrainingState::Fighting||stamina<cost||dodge_time>0)return false;
    guard=false;stamina-=cost;regen_delay=.45;attack_direction=direction;
    if(hits+misses>0&&last_attack==direction)repeated++;else repeated=0;
    last_attack=direction;direction_mask|=1u<<unsigned(direction);
    // The opponent reads repeated attacks, not a prescribed QTE sequence.
    if(repeated>=1&&enemy_phase==TrainingEnemyPhase::Measure)enemy_guard=direction;
    if(chain){++combo_count;++combo_links;}else combo_count=1;
    ++attack_start_serial;clear_buffer();
    player_phase=TrainingPlayerPhase::Windup;player_time=0;
    player_duration=chain?chain_windup_seconds:direction==TrainingDirection::Thrust?thrust_windup_seconds:cut_windup_seconds;
    player_resolved=false;player_swoosh=false;return true;
}
bool WorldTrainingCombat::attack(){
    if(state!=TrainingState::Fighting||dodge_time>0){++rejected_attacks;return false;}
    if(player_phase==TrainingPlayerPhase::Ready){
        if(commit_attack(selected,false))return true;
        ++rejected_attacks;message("Out of breath - make space and recover");return false;
    }
    if(combo_count>=maximum_combo_swings||pending_attack){++rejected_attacks;return false;}
    pending_attack=true;pending_direction=selected;pending_time=attack_buffer_seconds;++buffered_attacks;return true;
}
bool WorldTrainingCombat::dodge(){
    if(state!=TrainingState::Fighting||dodge_cooldown>0||stamina<23||player_phase!=TrainingPlayerPhase::Ready)return false;
    guard=false;stamina-=23;regen_delay=.75;dodge_time=.26;dodge_cooldown=1.0;dodges++;message("Quick step",.35);return true;
}
void WorldTrainingCombat::next_enemy(TrainingEnemyPhase p,double duration){enemy_phase=p;enemy_time=0;enemy_duration=duration;enemy_resolved=false;enemy_swoosh=false;}
void WorldTrainingCombat::begin_round(){
    enemy_max_health=arena_mode?arena_enemy_health:trail_threat?90.f:48.f+round*14.f;enemy_health=enemy_max_health;
    enemy_guard=TrainingDirection((round+2)%5);guard=false;guard_age=100;
    if(!arena_mode){stamina=std::min(100.f,stamina+35);health=std::min(100.f,health+15);}
    player_phase=TrainingPlayerPhase::Ready;player_time=0;state=TrainingState::Fighting;
    next_enemy(TrainingEnemyPhase::Measure,.65);message("Read the knight's guard. Attack another angle.",1.5);
}
namespace {
TrainingOpponent legacy_opponent(const WorldTrainingCombat &c){
    TrainingOpponent o;o.health=c.enemy_health;o.max_health=c.enemy_max_health;
    o.direction=c.enemy_direction;o.guard=c.enemy_guard;o.phase=c.enemy_phase;
    o.time=c.enemy_time;o.duration=c.enemy_duration;o.rng=c.rng;
    o.resolved=c.enemy_resolved;o.swoosh=c.enemy_swoosh;
    o.hits_received=c.hits;o.hits_dealt=c.enemy_hits;o.swings=c.enemy_swings;
    o.parries_received=c.parries;o.blocks_received=c.blocks;o.blocks_made=c.guarded;
    return o;
}
void apply_legacy(WorldTrainingCombat &c,const TrainingOpponent &o){
    c.enemy_health=o.health;c.enemy_max_health=o.max_health;c.enemy_direction=o.direction;
    c.enemy_guard=o.guard;c.enemy_phase=o.phase;c.enemy_time=o.time;c.enemy_duration=o.duration;
    c.rng=o.rng;c.enemy_resolved=o.resolved;c.enemy_swoosh=o.swoosh;
}
void next_opponent(TrainingOpponent &o,TrainingEnemyPhase p,double duration){
    o.phase=p;o.time=0;o.duration=duration;o.resolved=o.swoosh=false;
}
unsigned opponent_random(TrainingOpponent &o,unsigned n){o.rng^=o.rng<<13;o.rng^=o.rng>>17;o.rng^=o.rng<<5;return o.rng%n;}
}
void WorldTrainingCombat::event(TrainingEvents *events,unsigned id,TrainingContactKind kind,TrainingDirection direction,float damage,bool finisher){
    TrainingContactEvent value{++contact_serial,attack_start_serial,id,kind,direction,damage,clock};value.finisher=finisher;
    if(!events)return;
    if(events->count<events->capacity)events->values[events->count++]=value;else ++events->overflow;
}
void WorldTrainingCombat::defeated(TrainingOpponent &o,unsigned id,TrainingEvents *events){
    if(o.health<=0&&!o.defeat_announced){o.defeat_announced=true;event(events,id,TrainingContactKind::EnemyDefeated,o.reaction_direction);}
}
WorldTrainingCombat WorldTrainingCombat::opponent_view(const TrainingOpponent &o)const{
    auto view=*this;apply_legacy(view,o);
    view.hits=o.hits_received;view.enemy_hits=o.hits_dealt;view.enemy_swings=o.swings;
    view.parries=o.parries_received;view.blocks=o.blocks_received;view.guarded=o.blocks_made;
    view.attack_direction=o.reaction_direction;
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
    if(!shared_arena||state!=TrainingState::Fighting||id>=8||o.health<=0||
       !std::isfinite(damage)||damage<=0||damage>10000||unsigned(direction)>4)return false;
    const float actual=std::min(o.health,damage);o.health-=actual;o.reaction_direction=direction;
    ++o.hits_received;++hits;event(&events,id,TrainingContactKind::PlayerBody,direction,actual,o.health<=0);
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
    if(!shared_arena||!std::isfinite(dt)||dt<0||count>8||(count&&!targets))return false;
    for(std::size_t i=0;i<count;++i){
        const auto &t=targets[i];if(!t.opponent||t.id>=8||!std::isfinite(t.distance)||t.distance<0||
            !std::isfinite(t.opponent->health)||!std::isfinite(t.opponent->max_health)||t.opponent->health<0||
            t.opponent->max_health<=0||t.opponent->health>t.opponent->max_health||
            !std::isfinite(t.opponent->time)||!std::isfinite(t.opponent->duration)||t.opponent->time<0||
            t.opponent->duration<=0||unsigned(t.opponent->phase)>4||unsigned(t.opponent->direction)>4||unsigned(t.opponent->guard)>4)return false;
        for(std::size_t j=0;j<i;++j)if(targets[j].id==t.id||targets[j].opponent==t.opponent)return false;
    }
    if(paused){guard=false;clear_buffer();return true;}
    double remaining=std::min(dt,.25);
    while(remaining>1e-9){const double h=std::min(remaining,1./240.);if(impact_pause>0)impact_pause=std::max(0.,impact_pause-h);else shared_step(h,targets,count,&events);remaining-=h;}
    return true;
}
void WorldTrainingCombat::step(double dt,double distance){
    auto o=legacy_opponent(*this);TrainingTarget target{&o,0,distance};
    shared_step(dt,&target,1,nullptr);apply_legacy(*this,o);
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
    if(pending_attack){pending_time-=dt;if(pending_time<=0){clear_buffer();++expired_buffers;}}
    if(regen_delay<=0&&player_phase==TrainingPlayerPhase::Ready)stamina=std::min(100.f,stamina+float(dt*(guard?guard_stamina_per_second:ready_stamina_per_second+(watch_reward?7:0))*town_recovery));
    for(std::size_t i=0;i<count;++i){
        auto &o=*targets[i].opponent;
        if(o.seen_attack_serial!=attack_start_serial){o.seen_attack_serial=attack_start_serial;if(repeated>=1&&o.phase==TrainingEnemyPhase::Measure)o.guard=attack_direction;}
    }
    if(player_phase!=TrainingPlayerPhase::Ready){
        player_time+=dt;
        if(player_phase==TrainingPlayerPhase::Windup&&!player_swoosh&&player_time>=player_duration*.42){
            player_swoosh=true;++player_swings;event(events,8,TrainingContactKind::PlayerSwing,attack_direction);
        }
        if(player_phase==TrainingPlayerPhase::Windup&&player_time>=player_duration){
            player_phase=TrainingPlayerPhase::Strike;player_time-=player_duration;player_duration=player_strike_seconds;
            // One committed sword strike resolves against its closest actual
            // reachable defender. No locked target, rear hit or eight-way mint.
            TrainingTarget *chosen=nullptr;
            for(std::size_t i=0;i<count;++i){auto &t=targets[i];if(t.opponent->health<=0||!t.in_player_arc||t.distance>player_reach||t.distance<1.)continue;
                if(!chosen||t.distance<chosen->distance-1e-9||(std::abs(t.distance-chosen->distance)<=1e-9&&t.id<chosen->id))chosen=&t;
            }
            if(!chosen){++misses;message("Out of reach - face a challenger and step into range");}
            else{
                auto &o=*chosen->opponent;
                const bool opening=o.phase==TrainingEnemyPhase::Recovery||o.phase==TrainingEnemyPhase::Stagger;
                if(!opening&&o.guard==attack_direction){++misses;++guarded;++o.blocks_made;if(impact_pause_enabled)impact_pause=std::max(impact_pause,.025);event(events,chosen->id,TrainingContactKind::EnemyBlock,attack_direction);message("STEEL CLASH - change your attack angle",.8);}
                else{
                    const bool riposte=o.phase==TrainingEnemyPhase::Stagger;
                    const float damage=std::min(o.health,(riposte?25.f:15.f)*(story_assist?1.25f:1.f)*town_damage);
                    o.health-=damage;o.reaction_direction=attack_direction;++hits;++o.hits_received;
                    if(impact_pause_enabled)impact_pause=std::max(impact_pause,riposte?.045:.032);
                    event(events,chosen->id,TrainingContactKind::PlayerBody,attack_direction,damage,o.health<=0);defeated(o,chosen->id,events);
                    message(riposte?"RIPOSTE - BODY HIT":"BODY HIT",.7);
                }
            }
            player_resolved=true;
        }else if(player_phase==TrainingPlayerPhase::Strike&&player_time>=player_duration){player_phase=TrainingPlayerPhase::Recovery;player_time-=player_duration;player_duration=player_recovery_seconds;}
        else if(player_phase==TrainingPlayerPhase::Recovery){
            if(pending_attack&&combo_count<maximum_combo_swings&&player_time>=combo_link_seconds){
                const auto direction=pending_direction;
                if(!commit_attack(direction,true)){clear_buffer();message("Out of breath - combo ends",.8);}
            }
            if(player_phase==TrainingPlayerPhase::Recovery&&player_time>=player_duration){player_phase=TrainingPlayerPhase::Ready;player_time=0;combo_count=0;clear_buffer();}
        }
    }
    for(std::size_t i=0;i<count&&state==TrainingState::Fighting;++i){
        auto &t=targets[i];auto &o=*t.opponent;if(o.health<=0){defeated(o,t.id,events);continue;}
        o.time+=dt;
        if(o.phase==TrainingEnemyPhase::Windup&&!o.swoosh&&o.time>=o.duration*.68){o.swoosh=true;++o.swings;++enemy_swings;event(events,t.id,TrainingContactKind::EnemySwing,o.direction);}
        if(o.time<o.duration)continue;
        switch(o.phase){
        case TrainingEnemyPhase::Measure:
            if(arena_mode&&t.distance>enemy_reach+.05){next_opponent(o,TrainingEnemyPhase::Measure,.08);break;}
            o.direction=TrainingDirection(opponent_random(o,5));o.guard=TrainingDirection((unsigned(o.direction)+2+round)%5);
            next_opponent(o,TrainingEnemyPhase::Windup,std::max(.50,.72-round*.10));break;
        case TrainingEnemyPhase::Windup:
            next_opponent(o,TrainingEnemyPhase::Strike,.11);
            if(t.distance>enemy_reach)message("Knight missed - close in and counter!",.9);
            else if(dodge_time>0)message("Evaded - counter now!",.9);
            else if(t.player_can_guard&&guard&&selected==o.direction&&player_phase==TrainingPlayerPhase::Ready&&stamina>=8){
                if(guard_age<=parry_window_seconds){stamina-=7;++parries;++o.parries_received;if(impact_pause_enabled)impact_pause=std::max(impact_pause,.045);next_opponent(o,TrainingEnemyPhase::Stagger,parry_stagger_seconds);event(events,t.id,TrainingContactKind::PlayerParry,o.direction);message("STEEL PARRY - RIPOSTE!",parry_stagger_seconds);}
                else if(stamina>=22){stamina-=22;++blocks;++o.blocks_received;if(impact_pause_enabled)impact_pause=std::max(impact_pause,.025);event(events,t.id,TrainingContactKind::PlayerBlock,o.direction);message("STEEL BLOCK - release guard to recover",.8);}
                else{
                    stamina=0;const float damage=std::min(health,(18+round*3.f)*(story_assist?.45f:1.f)*town_resistance);health-=damage;++enemy_hits;++o.hits_dealt;clear_buffer();
                    if(impact_pause_enabled)impact_pause=std::max(impact_pause,.04);
                    guard=false;event(events,t.id,TrainingContactKind::EnemyBody,o.direction,damage,health<=0);message("GUARD BROKEN - BODY HIT",1.);
                }
                regen_delay=.4;
            }else{
                const float damage=std::min(health,(18+round*3.f)*(story_assist?.45f:1.f)*town_resistance);health-=damage;++enemy_hits;++o.hits_dealt;clear_buffer();
                if(impact_pause_enabled)impact_pause=std::max(impact_pause,.04);
                event(events,t.id,TrainingContactKind::EnemyBody,o.direction,damage,health<=0);message(guard?"BODY HIT - wrong guard angle":"BODY HIT - defend the incoming direction",1.);
            }
            if(health<=0){state=TrainingState::Lost;guard=false;clear_buffer();message("Yield. No injury - press E to retry or Esc to leave",8);}break;
        case TrainingEnemyPhase::Strike:next_opponent(o,TrainingEnemyPhase::Recovery,std::max(.40,.50-round*.05));break;
        case TrainingEnemyPhase::Recovery:case TrainingEnemyPhase::Stagger:next_opponent(o,TrainingEnemyPhase::Measure,.35+opponent_random(o,21)*.01);break;
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

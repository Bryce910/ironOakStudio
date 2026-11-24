// Arena-owned NPC-vs-NPC clocks. Player clocks remain solely TrainingCombat's.
    static unsigned team_random(TrainingOpponent&o,unsigned count){o.rng^=o.rng<<13;o.rng^=o.rng>>17;o.rng^=o.rng<<5;return o.rng%count;}
    static void team_phase(TrainingOpponent&o,TrainingEnemyPhase phase,double duration){o.phase=phase;o.time=0;o.duration=duration;o.resolved=o.swoosh=false;if(phase==TrainingEnemyPhase::Windup)o.attack_windup_seconds=duration;}
    void choose_team_targets(Vec3 player){
        for(unsigned id=0;id<challenger_count;++id){if(!npc_live(id)||opponents_[id].phase!=TrainingEnemyPhase::Measure)continue;
            const auto&at=challengers_[id].position;unsigned selected=id<teammate_first?player_target:no_target;
            double best=id<teammate_first?std::hypot(at.x-player.x,at.z-player.z):std::numeric_limits<double>::infinity();
            for(unsigned other=0;other<challenger_count;++other)if(opposite(id,other)&&npc_live(other)){
                const auto&to=challengers_[other].position;double distance=std::hypot(at.x-to.x,at.z-to.z);
                if(distance<best-.10||(selected==targets_[id]&&distance<best+.10)){best=distance;selected=other;}
            }targets_[id]=selected;
        }
    }
    void team_event(unsigned attacker,unsigned defender,TrainingContactKind kind,float damage,const TrainingAnatomicalHit&hit,const WorldTrainingCombat&fight){
        if(team_events_.count>=TeamContactEvents::capacity){++team_events_.overflow;return;}
        auto&e=team_events_.values[team_events_.count++];e={};e.serial=++team_serial_;e.attack_serial=opponents_[attacker].attack_start_serial;e.attacker_id=attacker;e.defender_id=defender;e.kind=kind;e.direction=opponents_[attacker].direction;e.damage=damage;e.time=fight.clock;e.body_part=hit.part;std::copy_n(hit.point,3,e.point.begin());
    }
    void resolve_team(unsigned attacker,unsigned defender,double begin,double end,double interval,WorldTrainingCombat&fight){
        auto&a=opponents_[attacker];auto&d=opponents_[defender];
        if(a.resolved||!opposite(attacker,defender)||!npc_live(defender)||challengers_[defender].state!=ChallengerState::Active)return;
        const auto&from=challengers_[attacker];const auto&to=challengers_[defender];
        if(std::hypot(from.position.x-to.position.x,from.position.z-to.position.z)>WorldTrainingCombat::enemy_reach+.8){if(end>=.60)a.resolved=true;return;}
        TeamContactQuery query;query.attacker_id=attacker;query.defender_id=defender;query.direction=a.direction;query.attack_serial=a.attack_start_serial;query.phase_begin=begin;query.phase_end=end;query.interval_seconds=interval;query.clock=fight.clock;query.strike_seconds=a.phase==TrainingEnemyPhase::Strike?a.duration:world_combat_timing::incoming_strike(unsigned(a.direction),a.attack_windup_seconds,fight.round);query.attacker_position={from.position.x,from.position.y,from.position.z};query.defender_position={to.position.x,to.position.y,to.position.z};query.attacker_yaw=from.yaw;query.defender_yaw=to.yaw;query.attacker_state=a;query.defender_state=d;
        TrainingAnatomicalHit hit;
        bool valid=team_query_&&team_query_(query,hit,team_context_)&&hit.valid&&std::isfinite(hit.sweep_t)&&hit.sweep_t>=0&&hit.sweep_t<=1&&std::isfinite(hit.attack_phase)&&(hit.attack_phase==-1||(hit.attack_phase>=0&&hit.attack_phase<=1));
        valid=valid&&std::isfinite(hit.defender_attack_phase)&&(hit.defender_attack_phase==-1||(hit.defender_attack_phase>=0&&hit.defender_attack_phase<=1))&&std::isfinite(hit.defender_pose_weight)&&hit.defender_pose_weight>=0&&hit.defender_pose_weight<=1;
        for(double x:hit.point)valid=valid&&std::isfinite(x)&&std::abs(x)<=1e7;
        valid=valid&&(hit.weapon_contact||(unsigned(hit.part)>=1&&unsigned(hit.part)<=8));
        if(!valid){if(end>=.60)a.resolved=true;return;}
        a.resolved=true;
        if(hit.weapon_contact){
            a.blocked_attack_serial=a.attack_start_serial;a.blocked_attack_phase=hit.attack_phase>=0?hit.attack_phase:begin+(end-begin)*hit.sweep_t;a.blocked_attack_time=fight.clock;a.blocked_pose_slot=~0u;a.blocked_pose_weight=0;
            ++a.blocks_received;++d.blocks_made;team_phase(a,TrainingEnemyPhase::Recovery,.34);a.resolved=true;
            if(d.phase==TrainingEnemyPhase::Windup||d.phase==TrainingEnemyPhase::Strike){d.blocked_attack_serial=d.attack_start_serial;d.blocked_attack_phase=hit.defender_attack_phase>=0?hit.defender_attack_phase:.36;d.blocked_attack_time=fight.clock;d.blocked_pose_slot=hit.defender_pose_slot;d.blocked_pose_weight=hit.defender_pose_weight;team_phase(d,TrainingEnemyPhase::Recovery,.34);d.resolved=true;}
            team_event(attacker,defender,TrainingContactKind::EnemyBlock,0,hit,fight);return;
        }
        // NPC damage is bounded ordinary body damage, including thrusts. A
        // teammate does not gain the player's lethal-thrust/sever privileges.
        const float damage=std::min(d.health,18.f);d.health-=damage;++a.hits_dealt;++d.hits_received;d.reaction_direction=a.direction;
        team_event(attacker,defender,TrainingContactKind::EnemyBody,damage,hit,fight);
        if(d.health<=0){d.health=0;d.defeat_announced=true;d.guard_reaction_pending=false;team_event(attacker,defender,TrainingContactKind::EnemyDefeated,0,hit,fight);}
    }
    void step_teams(double dt,WorldTrainingCombat&fight){
        for(unsigned id=0;id<challenger_count;++id){if(!npc_live(id)||challengers_[id].state!=ChallengerState::Active||fixture_pose_hold_[id]||(id<teammate_first&&targets_[id]==player_target))continue;
            auto&o=opponents_[id];const unsigned victim=targets_[id];
            if(!world_combat_anatomy::can_fight(o.severed_parts)){team_phase(o,TrainingEnemyPhase::Measure,.25);continue;}
            const bool target_valid=victim<challenger_count&&opposite(id,victim)&&npc_live(victim)&&challengers_[victim].state==ChallengerState::Active;
            if(target_valid){const auto&target=opponents_[victim];if(target.phase==TrainingEnemyPhase::Windup||target.phase==TrainingEnemyPhase::Strike){if(team_seen_[id]!=target.attack_start_serial){team_seen_[id]=target.attack_start_serial;team_guard_at_[id]=fight.clock+world_combat_difficulty::reaction_delay_s(config_.difficulty,false);}if(fight.clock>=team_guard_at_[id])o.guard=target.direction;}}
            const double strike=o.phase==TrainingEnemyPhase::Strike?o.duration:world_combat_timing::incoming_strike(unsigned(o.direction),o.attack_windup_seconds,fight.round);
            auto phase=[&](){if(o.phase==TrainingEnemyPhase::Windup)return o.time<=o.duration?.36*std::clamp(o.time/std::max(.001,o.duration),0.,1.):.36+.24*std::clamp((o.time-o.duration)/strike,0.,1.);if(o.phase==TrainingEnemyPhase::Strike)return .36+.24*std::clamp(o.time/std::max(.001,o.duration),0.,1.);return 0.;};
            double before=phase();o.time+=dt;
            if(target_valid&&!o.resolved&&(o.phase==TrainingEnemyPhase::Windup||o.phase==TrainingEnemyPhase::Strike)){double after=phase(),lo=std::max(.26,before),hi=std::min(.60,after);if(hi>=lo&&after>=.26)resolve_team(id,victim,lo,hi,dt*(hi-lo)/std::max(1e-12,after-before),fight);}
            if(o.phase==TrainingEnemyPhase::Windup&&!o.swoosh&&o.time>=o.duration*.68){o.swoosh=true;++o.swings;}
            if(o.time<o.duration)continue;
            switch(o.phase){
                case TrainingEnemyPhase::Measure:{double distance=target_valid?std::hypot(challengers_[id].position.x-challengers_[victim].position.x,challengers_[id].position.z-challengers_[victim].position.z):100.;if(distance>team_commit_reach){team_phase(o,TrainingEnemyPhase::Measure,.08);break;}o.direction=TrainingDirection(team_random(o,5));++o.attack_start_serial;team_phase(o,TrainingEnemyPhase::Windup,world_combat_timing::incoming(unsigned(o.direction),config_.difficulty,fight.round).windup);break;}
                case TrainingEnemyPhase::Windup:{const bool resolved=o.resolved;const double overflow=std::max(0.,o.time-o.duration);team_phase(o,TrainingEnemyPhase::Strike,strike);o.time=overflow;o.resolved=resolved;break;}
                case TrainingEnemyPhase::Strike:team_phase(o,TrainingEnemyPhase::Recovery,world_combat_timing::incoming_recovery(o.attack_windup_seconds,fight.round));break;
                case TrainingEnemyPhase::Recovery:case TrainingEnemyPhase::Stagger:team_phase(o,TrainingEnemyPhase::Measure,(.35+team_random(o,21)*.01)/world_combat_difficulty::profile(config_.difficulty).attack_tempo_scale);break;
            }
        }
    }

#include "engine/world_blood_effects.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace world_blood_effects {
namespace {
void add(std::uint64_t &value,std::uint64_t amount=1){
    value+=std::min(amount,std::numeric_limits<std::uint64_t>::max()-value);
}
bool point_valid(Point p){
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&
        std::abs(p.x)<1e8&&std::abs(p.y)<1e7&&std::abs(p.z)<1e8;
}
bool contact_valid(const Contact &c){
    if(!c.valid||!point_valid(c.position)||!point_valid(c.direction)||
       !std::isfinite(c.floor_y)||std::abs(c.floor_y)>=1e7||!std::isfinite(c.strength)||c.strength<.25f||c.strength>2.f)return false;
    const double length=std::hypot(c.direction.x,c.direction.y,c.direction.z);
    return std::isfinite(length)&&length>1e-8;
}
template<class T,std::size_t N>T &slot(std::array<T,N> &pool,std::uint64_t &replaced){
    const auto free=std::find_if(pool.begin(),pool.end(),[](const T &p){return p.duration<=0;});
    if(free!=pool.end())return *free;
    add(replaced);
    return *std::min_element(pool.begin(),pool.end(),[](const T &a,const T &b){return a.serial<b.serial;});
}
template<class T,std::size_t N>void advance(std::array<T,N> &pool,double dt){
    for(auto &p:pool)if(p.duration>0){
        p.age+=dt;
        if(p.age>=p.duration)p=T{};
    }
}
void advance_stains(std::array<Stain,stain_capacity> &pool,double dt){
    for(auto &p:pool)if(p.duration>0){
        if(p.persistent)p.age=std::min(p.age+dt,maximum_persistent_stain_age);
        else{p.age+=dt;if(p.age>=p.duration)p=Stain{};}
    }
}
}

bool System::configure(const Config &config){
    if(!std::isfinite(config.mesh_duration)||config.mesh_duration<1./240.||config.mesh_duration>30||
       !std::isfinite(config.stain_duration)||config.stain_duration<.1||config.stain_duration>120||config.variant_count>config.variant_durations.size()){
        add(stats_.rejected_configurations);return false;
    }
    for(unsigned i=0;i<config.variant_count;++i)if(!std::isfinite(config.variant_durations[i])||config.variant_durations[i]<1./240.||config.variant_durations[i]>30){add(stats_.rejected_configurations);return false;}
    config_=config;return true;
}
void System::refresh_counts(){
    stats_.active_meshes=stats_.active_stains=0;
    for(const auto &p:meshes_)stats_.active_meshes+=p.duration>0;
    for(const auto &p:stains_)stats_.active_stains+=p.duration>0;
}
void System::emit(const Contact &contact,unsigned defender,bool neck_jet){
    const double length=std::hypot(contact.direction.x,contact.direction.y,contact.direction.z);
    const Point forward{contact.direction.x/length,contact.direction.y/length,contact.direction.z/length};
    const double yaw=std::atan2(forward.x,forward.z);
    add(hit_id_);
    for(unsigned lobe=0;lobe<2;++lobe){
        add(serial_);const double delay=lobe?(neck_jet?.012:.035):0;
        const double duration=config_.variant_count?config_.variant_durations[(hit_id_-1+lobe)%config_.variant_count]:config_.mesh_duration;
        auto &mesh=slot(meshes_,stats_.mesh_replaced);
        mesh={contact.position,forward,yaw,0,(neck_jet?std::min(.24,duration):duration)+delay,serial_,defender,hit_id_,lobe,contact.strength,contact.finisher,delay,contact.floor_y,neck_jet};
        add(stats_.mesh_spawned);if(lobe)add(stats_.secondary_spawned);
    }
    auto &stain=slot(stains_,stats_.stains_replaced);
    stain={{contact.position.x+(neck_jet?forward.x*.7:0),contact.floor_y,contact.position.z+(neck_jet?forward.z*.7:0)},yaw,0,config_.stain_duration,hit_id_,defender,hit_id_,contact.strength,contact.finisher,config_.persistent_stains};
    add(stats_.stains_spawned);add(stats_.impacts_spawned);if(contact.finisher)add(stats_.finisher_events);
    stats_.max_strength=std::max(stats_.max_strength,contact.strength);if(neck_jet)add(stats_.neck_spurts);
}
bool System::update(const Input &input,double delta){
    if(!std::isfinite(delta)||delta<0||!std::isfinite(input.combat_clock)||input.combat_clock<0||
       input.event_count>64||(input.event_count&&!input.events)||input.visual_spurt_count>16||(input.visual_spurt_count&&!input.visual_spurts)){
        add(stats_.invalid_inputs);return false;
    }
    for(unsigned i=0;i<input.event_count;++i)if(!input.events[i].serial||input.events[i].defender>1){add(stats_.invalid_inputs);return false;}
    if(input.paused)return true;
    auto spurts=[&](){for(unsigned i=0;i<input.visual_spurt_count;++i){const auto &c=input.visual_spurts[i];if(contact_valid(c))emit(c,1,true);else add(stats_.invalid_contacts);}};
    const double dt=std::min(delta,maximum_step);
    stats_.elapsed+=dt;
    advance(meshes_,dt);advance_stains(stains_,dt);
    if(have_clock_&&input.combat_clock<last_clock_)add(stats_.clock_rebases);
    last_clock_=input.combat_clock;have_clock_=true;
    const std::array<std::uint64_t,2> counters={input.outgoing_hits,input.incoming_hits};
    if(input.resolved_events){
        unsigned emitted[2]={};
        for(unsigned i=0;i<input.event_count;++i){
            const auto &e=input.events[i];if(e.serial<=event_seen_)continue;event_seen_=e.serial;
            add(e.defender?stats_.outgoing_events:stats_.incoming_events);
            if(!contact_valid(e.contact)){add(stats_.invalid_contacts);continue;}
            if(emitted[e.defender]++>=events_per_defender_limit){add(stats_.limited_events);continue;}
            emit(e.contact,e.defender);
        }
        seen_=counters;spurts();refresh_counts();return true;
    }
    bool rebased=false;
    for(unsigned event=0;event<2;++event){
        const auto current=counters[event];
        if(current<seen_[event]){seen_[event]=current;rebased=true;continue;}
        const auto count=current-seen_[event];seen_[event]=current;
        if(!count)continue;
        add(event?stats_.incoming_events:stats_.outgoing_events,count);
        const unsigned defender=1-event;
        if(!contact_valid(input.contacts[defender])){add(stats_.invalid_contacts,count);continue;}
        const auto bounded=std::min<std::uint64_t>(count,events_per_defender_limit);
        add(stats_.limited_events,count-bounded);
        for(std::uint64_t n=0;n<bounded;++n)emit(input.contacts[defender],defender);
    }
    if(rebased)add(stats_.counter_rebases);
    spurts();refresh_counts();return true;
}
bool System::rebase(std::uint64_t outgoing,std::uint64_t incoming,double clock){
    if(!std::isfinite(clock)||clock<0){add(stats_.invalid_inputs);return false;}
    seen_={outgoing,incoming};last_clock_=clock;have_clock_=true;add(stats_.counter_rebases);return true;
}
void System::reset(std::uint64_t outgoing,std::uint64_t incoming){
    meshes_={};stains_={};seen_={outgoing,incoming};event_seen_=0;last_clock_=0;have_clock_=false;
    stats_.active_meshes=stats_.active_stains=0;stats_.elapsed=0;add(stats_.resets);
}
Stage System::stage()const{
    Stage result;
    for(const auto &p:meshes_)if(p.duration>0)result.meshes[result.mesh_count++]=p;
    for(const auto &p:stains_)if(p.duration>0)result.stains[result.stain_count++]=p;
    return result;
}
}

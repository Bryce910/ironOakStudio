#include "engine/world_opening_prologue.h"
#include <iostream>
#include <limits>
#include <sstream>
namespace light=world_opening_room_lights;
namespace story=world_opening_prologue;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
int main(){
    for(unsigned room=0;room<4;++room){
        const auto &s=light::switches[room];
        light::Vec eye{s.position.x+s.normal.x*.6,s.position.y+.53,s.position.z+s.normal.z*.6};
        const light::Vec forward{s.position.x-eye.x,s.position.y-eye.y,s.position.z-eye.z};
        CHECK(light::target(eye,forward)==int(room));
        CHECK(light::target(eye,{-forward.x,-forward.y,-forward.z})==-1);
        CHECK(light::target(eye,{0,0,0})==-1);
        const light::Vec behind{s.position.x-s.normal.x*.6,s.position.y+.53,s.position.z-s.normal.z*.6};
        CHECK(light::target(behind,{s.position.x-behind.x,s.position.y-behind.y,s.position.z-behind.z})==-1);
        const light::Vec far{s.position.x+s.normal.x*2,s.position.y+.53,s.position.z+s.normal.z*2};
        CHECK(light::target(far,{s.position.x-far.x,s.position.y-far.y,s.position.z-far.z})==-1);
        light::Controls controls;
        CHECK(controls.tick(false,false,int(room))==-1);
        CHECK(controls.tick(true,true,int(room))==-1); // cinematic-held button
        CHECK(controls.tick(true,false,int(room))==-1);
        CHECK(controls.tick(true,true,int(room))==int(room));
        for(unsigned i=0;i<20;++i)CHECK(controls.tick(true,true,int(room))==-1);
        CHECK(controls.toggles[room]==1);
        controls.tick(false,true,int(room));
        CHECK(controls.tick(true,true,int(room))==-1); // pause-held button
        controls.tick(true,false,int(room));
        CHECK(controls.tick(true,true,int(room))==int(room));
        CHECK(controls.toggles[room]==2);
        controls.release();
        CHECK(controls.tick(true,true,int(room))==-1);
    }
    // A bathroom switch cannot be reached through its perpendicular front wall.
    const auto &bath=light::switches[3].position;
    const light::Vec outside{-3495.78,601.93,-10998.0};
    CHECK(light::target(outside,{bath.x-outside.x,bath.y-outside.y,bath.z-outside.z})==-1);
    const double nan=std::numeric_limits<double>::quiet_NaN();
    CHECK(light::target({nan,601.93,-10996.1},{1,0,0})==-1);
    for(const auto &p:light::practicals)CHECK(light::practical_room(p.position)==int(p.room));
    CHECK(light::practical_room({-3495.3,602.15,-10996.9})==-1); // window daylight
    CHECK(light::practical_room({nan,0,0})==-1);
    story::Config config;story::State state;std::string error;
    CHECK(state.configure(config,error));
    for(unsigned mask=0;mask<16;++mask){
        CHECK(state.set_room_lights(mask));
        auto saved=state.stable_save();CHECK(saved.room_lights==mask);
        std::string bytes;CHECK(story::encode_save(saved,config,bytes,error));
        story::Save parsed;std::istringstream input(bytes);CHECK(story::parse_save(input,config,parsed,error));
        CHECK(parsed.room_lights==mask);CHECK(state.restore(parsed,error));
        state.retry();CHECK(state.room_lights()==mask);
        light::render_mask(true,mask);for(unsigned room=0;room<4;++room)CHECK(light::emission_weights[room]==(light::on(mask,room)?1.f:0.f));
    }
    CHECK(!state.set_room_lights(16)&&state.room_lights()==15);
    auto old=story::checkpoint_save(story::Checkpoint::HouseStart,config);
    std::string good;CHECK(story::encode_save(old,config,good,error));
    const auto at=good.find("room_lights");CHECK(at!=std::string::npos);
    std::istringstream legacy(good.substr(0,at));story::Save parsed;
    CHECK(story::parse_save(legacy,config,parsed,error)&&parsed.room_lights==15);
    for(const auto &suffix:{"room_lights 16\n","room_lights -1\n","room_lights 4294967296\n","room_lights\n","room_lights 3\nroom_lights 4\n","unknown 3\n"}){
        std::istringstream corrupt(good.substr(0,at)+suffix);parsed.room_lights=7;
        CHECK(!story::parse_save(corrupt,config,parsed,error));CHECK(parsed.room_lights==7);
    }
    state.reset();CHECK(state.room_lights()==15);
    light::render_mask(false,0);for(float weight:light::emission_weights)CHECK(weight==1);
    std::cout<<"world_opening_room_lights: "<<checks<<" targeting, circuit, debounce, pause and checkpoint compatibility/corruption checks passed\n";
}

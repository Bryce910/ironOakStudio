// Weather configuration is prepared on explicit Apply. The frame path only
// touches fixed packed arrays; it performs no file IO or project copies.
world_weather_authoring::Global authored_weather_global;
std::vector<world_weather_authoring::BoundArea> authored_weather_areas;
world_weather_authoring::Transition authored_weather_transition;
world_weather_wetness::State authored_wetness_state;
world_storm::Controller authored_storm;
world_storm::Output authored_storm_output;
WorldLightningVisual lightning_visual;
bool lightning_ready=lightning_visual.create(),weather_thunder_ready=false;
unsigned authored_weather_commits=0,authored_weather_failures=lightning_ready?0:1;
unsigned long long authored_weather_frames=0;
double authored_weather_clock=0;
float lightning_history_flash=0;
auto world_weather_prepare=[&](const world_studio::Document &document,std::vector<world_weather_authoring::BoundArea>&next,std::string &error){
    return world_weather_authoring::valid(document.weather_global,error)&&world_authoring::weather_areas(document,next,error);
};
auto world_weather_commit=[&](const world_studio::Document &document,std::vector<world_weather_authoring::BoundArea> next,const Camera *eye=nullptr){
    authored_weather_global=document.weather_global;authored_weather_areas=std::move(next);++authored_weather_commits;
    if(!weather_drips_prepared&&(document.weather_global.enabled||!document.weather_areas.empty()))prepare_weather_drips(eye?*eye:cam);
};
{
    std::string error;std::vector<world_weather_authoring::BoundArea> next;
    if(world_weather_prepare(studio.live,next,error))world_weather_commit(studio.live,std::move(next));
    else{++authored_weather_failures;studio.message="Weather could not load: "+error;}
    // Optional built-in effect shares the existing demo audio owner. A missing
    // file cannot disable map sounds, footsteps, or the legacy ambience bank.
    if(demo_audio){char*base=SDL_GetBasePath();if(base){
        const auto root=std::filesystem::u8path(base);SDL_free(base);
        const std::array<std::string,4> paths={(root/"audio/weather/near-crack-01.wav").u8string(),(root/"audio/weather/near-crack-02.wav").u8string(),(root/"audio/weather/distant-rumble-01.wav").u8string(),(root/"audio/weather/distant-rumble-02.wav").u8string()};
        WorldDemoThunderBank bank;bank.near_count=bank.distant_count=2;for(unsigned i=0;i<2;++i){bank.near_paths[i]=paths[i].c_str();bank.distant_paths[i]=paths[i+2].c_str();}
        char error_text[256]={};weather_thunder_ready=world_demo_audio_preload_thunder_bank(demo_audio,bank,error_text,sizeof(error_text));
        if(!weather_thunder_ready){const auto fallback=(root/"audio/weather-thunder.wav").u8string();weather_thunder_ready=world_demo_audio_preload_thunder(demo_audio,fallback.c_str(),error_text,sizeof(error_text));}
        if(!weather_thunder_ready)std::fprintf(stderr,"[Weather] Thunder unavailable: %s\n",error_text);
    }}
}
auto authored_weather_at=[&](double x,double z){return world_weather_authoring::sample(active_authored_weather,x,z);};
auto authored_weather_frame=[&](const Camera&listener,float seconds,bool focused,bool paused){
    using namespace world_weather_authoring;
    const bool permitted=focused&&!paused&&!destination_loading;
    if(permitted)authored_weather_clock+=std::clamp(double(seconds),0.,1.);
    Sample legacy;legacy.rain=weather_disabled?0:std::clamp(rain,0.f,1.f);legacy.cloud=cloud;legacy.fog=weather_disabled?0:mist;
    PackedFrame target;
    if(!pack(authored_weather_global,authored_weather_areas.data(),unsigned(authored_weather_areas.size()),legacy,authored_weather_clock,target)){++authored_weather_failures;target={};target.global=legacy;}
    transition(authored_weather_transition,target,permitted?seconds:0);active_authored_weather=authored_weather_transition.frame;
    if(weather_disabled){active_authored_weather={};active_authored_weather.global=legacy;}
    if(!world_weather_wetness::update(authored_wetness_state,active_authored_weather,legacy.rain,seconds,permitted,authored_weather_frames==0))++authored_weather_failures;
    active_weather_wetness=world_weather_wetness::packet(authored_wetness_state);
    std::array<world_storm::Source,world_storm::source_limit> sources{};unsigned count=0;
    const auto effective=authored_weather_at(listener.x,listener.z);
    // Alpha-over coefficients match the renderer's sorted area blend exactly.
    float remaining=1;std::array<float,area_limit> weights{};
    for(unsigned k=active_authored_weather.count;k>0;--k){const unsigned i=k-1;const float w=area_weight(active_authored_weather.areas[i],listener.x,listener.z);weights[i]=remaining*w;remaining*=1-w;}
    auto add=[&](std::uint64_t id,std::uint32_t seed,double x,double y,double z,float radius,const Sample&s,float weight,bool relative){if(weight<=1.e-5f||(!s.thunder_enabled&&!s.lightning_enabled))return;auto &a=sources[count++];a.id=id;a.seed=seed;a.x=x;a.y=y;a.z=z;a.radius=radius;a.weight=weight;a.interval=s.thunder_interval;a.flash_strength=s.flash_strength;a.thunder_volume=s.thunder_volume;a.lightning_weight=s.lightning_weight;a.thunder_weight=s.thunder_weight;a.lightning_enabled=s.lightning_enabled;a.thunder_enabled=s.thunder_enabled;a.listener_relative=relative;};
    if(active_authored_weather.global_authored)add(UINT64_MAX,authored_weather_global.profile.seed,0,0,0,350,active_authored_weather.global,remaining,true);
    for(unsigned i=0;i<active_authored_weather.count;++i){const auto&p=active_authored_weather.areas[i];Sample s;s.thunder_interval=p.effects[0];s.flash_strength=p.effects[1];s.thunder_volume=p.effects[2];s.thunder_weight=p.flags[0];s.lightning_weight=p.flags[1];s.thunder_enabled=s.thunder_weight>1.e-5f;s.lightning_enabled=s.lightning_weight>1.e-5f;std::uint32_t seed=1;double y=0;for(const auto&b:authored_weather_areas)if(b.id==p.id){seed=b.area.profile.seed;y=b.area.y;break;}add(p.id,seed,p.bounds[0],y,p.bounds[1],std::min(p.bounds[3],1500.f),s,weights[i],false);}
    // With no storm over the listener, keep one bounded set of genuinely
    // visible distant sources. Their origins pass the same alpha-over field;
    // a dry local override can never emit a global bolt inside the dry zone.
    if(!count&&(active_authored_weather.global_authored||active_authored_weather.authored_count)){
        for(unsigned slot=0;slot<=active_authored_weather.count;++slot){
            if(!slot&&!active_authored_weather.global_authored)continue;
            const auto presence=world_storm_lighting::source_presence(active_authored_weather,slot,listener.x,listener.z,authored_storm.sequence);
            if(presence.remote<=1.e-5f)continue;
            Sample s;std::uint64_t id=UINT64_MAX;std::uint32_t seed=authored_weather_global.profile.seed;double y=0;
            if(!slot)s=active_authored_weather.global;
            else{const auto&p=active_authored_weather.areas[slot-1];id=p.id;s.thunder_interval=p.effects[0];s.flash_strength=p.effects[1];s.thunder_volume=p.effects[2];s.thunder_weight=p.flags[0];s.lightning_weight=p.flags[1];s.thunder_enabled=s.thunder_weight>1.e-5f;s.lightning_enabled=s.lightning_weight>1.e-5f;for(const auto&b:authored_weather_areas)if(b.id==id){seed=b.area.profile.seed;y=b.area.y;break;}}
            y=std::max(y,double(weather_surface(presence.x,presence.z,nullptr)));
            add(id,seed,presence.x,y,presence.z,0,s,presence.remote,false);
        }
        float total=0;for(unsigned i=0;i<count;++i)total+=sources[i].weight;
        if(total>.35f)for(unsigned i=0;i<count;++i)sources[i].weight*=.35f/total;
    }
    world_storm::Frame frame;frame.dt=std::clamp(double(seconds),0.,1.);frame.x=listener.x;frame.y=listener.y;frame.z=listener.z;frame.focused=focused;frame.paused=paused;frame.loading=destination_loading;frame.enabled=!weather_disabled;
    if(!world_storm::update(authored_storm,sources.data(),count,frame,authored_storm_output))++authored_weather_failures;
    ++authored_weather_frames;return effective;
};

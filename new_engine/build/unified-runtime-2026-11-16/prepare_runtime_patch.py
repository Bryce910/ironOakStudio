from pathlib import Path
import shutil,json,hashlib
N=Path(__file__).resolve().parent;BASE=N.parent/'joust-articulated-falls-2026-11-16';SOURCE=N/'source-v1'
if SOURCE.is_symlink():SOURCE.unlink();shutil.copytree(BASE/'source-v1',SOURCE)
(N/'SOURCE_ORIGIN.json').write_text(json.dumps(dict(baseline=str(BASE/'source-v1'),baseline_runtime_sha256=hashlib.sha256((BASE/'runtime-native-v1/native-bin/rpg').read_bytes()).hexdigest(),canonical_source_edited=False),indent=2)+'\n')
build=(BASE/'build_native_candidate.py').read_text().replace('build/joust-reactions-audio-2026-11-16','build/joust-articulated-falls-2026-11-16')
(N/'build_native_candidate.py').write_text(build);shutil.copy2(BASE/'seed_native_cache.py',N/'seed_native_cache.py')
p=SOURCE/'engine/world_road_traffic_runtime.inc';s=p.read_text().replace('bool road_traffic_loaded=false,road_traffic_validated=false;','bool road_traffic_loaded=false,road_traffic_validated=false,road_traffic_route_failed=false;')
s=s.replace('if(outdoors&&!road_traffic_validated){','''// Destination collision/support owners settle only at the end of loading.
    // The apartment's support must never validate an outdoor route mid-transport.
    if(outdoors&&!destination_loading&&!road_traffic_validated&&!road_traffic_route_failed){''')
s=s.replace('''            launch->exit_code=1;running=false;return;''','''            // An artist can obstruct an ambient route. Keep the story playable,
            // quarantine the invalid traffic and report the actual bad sample.
            road_traffic_route_failed=true;active_road_traffic=nullptr;return;''',1)
p.write_text(s)
p=SOURCE/'engine/world_road_traffic_core.h';s=p.read_text().replace('bool initialized=false,validated=false,enabled=false;double clock=0;unsigned order=0;','''bool initialized=false,validated=false,enabled=false;double clock=0;unsigned order=0;
    const char *validation_reason="not checked";unsigned validation_bank=0;
    double validation_station=0;Point validation_point;''')
old='''        validated=false;if(!route.count)return false;for(unsigned bank=0;bank<6;++bank){const auto&f=profiles[bank];if(f.radius<=0||f.radius+route.config.lane_offset>route.config.road_half_width+.02||f.height<.5||f.height>5||f.front<0||f.rear<0||f.front+f.rear>9||f.stride<.1||f.stride>12)return false;
            Point old;bool have=false;for(double s=0;s<route.cycle;s+=.65){Point p=route.sample(s);double pitch;if(!floor(p,f,q,pitch)||protected_point(p,f,false))return false;if(have&&!q.clear(old.x,old.z,p.x,p.z,std::min(old.y,p.y),f.height,f.radius,q.context))return false;old=p;have=true;}}
        validated=true;return true;'''
new='''        validated=false;validation_reason="empty route";if(!route.count)return false;
        for(unsigned bank=0;bank<6;++bank){
            validation_bank=bank;const auto&f=profiles[bank];validation_reason="invalid profile or lane width";
            if(f.radius<=0||f.radius+route.config.lane_offset>route.config.road_half_width+.02||f.height<.5||f.height>5||f.front<0||f.rear<0||f.front+f.rear>9||f.stride<.1||f.stride>12)return false;
            Point old;bool have=false;
            for(double s=0;s<route.cycle;s+=.65){
                Point p=route.sample(s);validation_station=s;validation_point=p;double pitch;
                validation_reason="ground, slope or capsule support";if(!floor(p,f,q,pitch))return false;
                validation_reason="protected camp or town envelope";if(protected_point(p,f,false))return false;
                validation_reason="swept movement collision";if(have&&!q.clear(old.x,old.z,p.x,p.z,std::min(old.y,p.y),f.height,f.radius,q.context))return false;
                old=p;have=true;
            }
        }
        validated=true;validation_reason="ready";return true;'''
assert old in s;s=s.replace(old,new,1);p.write_text(s)
p=SOURCE/'engine/world_road_traffic.cpp';s=p.read_text();old='''bool world_road_traffic_validate_route(WorldRoadTraffic*p,const WorldRoadTrafficQuery&q,char*error,std::size_t cap){if(error&&cap)error[0]=0;if(!p||!p->ready||!p->core.validate(q)){error_text(error,cap,"Road traffic lane/turn lacks clear supported ground for actual asset bounds");return false;}return true;}'''
new='''bool world_road_traffic_validate_route(WorldRoadTraffic*p,const WorldRoadTrafficQuery&q,char*error,std::size_t cap){
    if(error&&cap)error[0]=0;
    if(!p||!p->ready){error_text(error,cap,"Road traffic assets are not ready");return false;}
    if(!p->core.validate(q)){
        const auto &c=p->core;char message[384];
        std::snprintf(message,sizeof message,"%s; bank%u station%.2f at %.3f %.3f %.3f",c.validation_reason,c.validation_bank,c.validation_station,c.validation_point.x,c.validation_point.y,c.validation_point.z);
        error_text(error,cap,message);return false;
    }
    return true;
}'''
assert old in s;s=s.replace(old,new,1);p.write_text(s)
print('Private common source prepared: defer ambient route validation until arrival loading finishes; blocked routes no longer terminate gameplay.')

#include "engine/world_opening_cinematic_settings.h"
#include <iostream>
#include <limits>
namespace story=world_opening_prologue;
namespace settings=world_opening_cinematic_settings;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
static std::string document(const std::vector<story::HouseShot> &shots){
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17)<<"VALWOOD_HOME_CINEMATIC 1\n";
    auto vec=[&](story::Vec v){out<<v.x<<' '<<v.y<<' '<<v.z<<' ';};
    for(const auto &s:shots){out<<"shot "<<s.duration<<' ';vec(s.rail.eye_from);vec(s.rail.eye_to);vec(s.rail.target_from);vec(s.rail.target_to);
        out<<s.rail.fov_from<<' '<<s.rail.fov_to<<' '<<std::quoted(s.subtitle)<<'\n';}
    out<<"end\n";return out.str();
}
static bool rejection_preserves(const std::string &bytes,const story::Config &base){
    story::HouseShot sentinel;sentinel.duration=7.;sentinel.subtitle="Preserve this previous output";
    std::vector<story::HouseShot> out={sentinel};const auto previous=document(out);std::string error="old error";
    std::istringstream input(bytes);
    if(settings::parse(input,base,out,error)||error.empty()||document(out)!=previous){
        std::cerr<<"Parser did not reject transactionally: "<<bytes.substr(0,160)<<"\nerror: "<<error<<'\n';return false;
    }
    return true;
}
int main(int argc,char **argv){
    story::Config base;story::HouseShot shot;shot.duration=3.2;shot.rail=base.house_rail;
    shot.subtitle="Another late shift. \"One quiet night.\"";
    const auto good=document({shot});std::vector<story::HouseShot> out;std::string error="old error";
    std::istringstream input(good);CHECK(settings::parse(input,base,out,error));CHECK(error.empty());
    CHECK(out.size()==1&&out[0].duration==shot.duration&&out[0].subtitle==shot.subtitle);
    CHECK(document(out)==good);CHECK(base.house_shots.empty());
    // Focus distance is not camera orientation. A collinear target with a
    // different positive distance must hand off without a camera rotation.
    {auto collinear=shot;const auto eye=collinear.rail.eye_to,target=collinear.rail.target_to;
        collinear.rail.target_to={eye.x+(target.x-eye.x)*.2,eye.y+(target.y-eye.y)*.2,eye.z+(target.z-eye.z)*.2};
        std::istringstream close_target(document({collinear}));CHECK(settings::parse(close_target,base,out,error));
        CHECK(out.size()==1&&out[0].rail.target_to.z==collinear.rail.target_to.z);}
    {auto backwards=shot;const auto eye=backwards.rail.eye_to,target=backwards.rail.target_to;
        backwards.rail.target_to={eye.x-(target.x-eye.x),eye.y-(target.y-eye.y),eye.z-(target.z-eye.z)};
        CHECK(rejection_preserves(document({backwards}),base));}
    std::vector<story::HouseShot> sequence(4,shot);
    for(unsigned i=0;i<sequence.size();++i){sequence[i].duration=2.+i;sequence[i].subtitle=i==3?"":"A short thought.";}
    std::istringstream multi(document(sequence));CHECK(settings::parse(multi,base,out,error));CHECK(out.size()==4&&out.back().subtitle.empty());
    for(const std::string bad:{"", "VALWOOD_HOME_CINEMATIC 1\nend\n", "VALWOOD_HOME_CINEMATIC 1\n", "wrong 1\nend\n",
        "VALWOOD_HOME_CINEMATIC 2\nend\n", "VALWOOD_HOME_CINEMATIC -4294967295\nend\n", "VALWOOD_HOME_CINEMATIC 4294967297\nend\n"})
        CHECK(rejection_preserves(bad,base));
    // A syntactically complete negative version must also reject (not merely
    // fail later because its shot list is empty).
    for(const std::string version:{"-4294967295","-1","+1","01","1.0","4294967297","18446744073709551617"}){
        auto text=good;text.replace(text.find(" 1\n"),3," "+version+"\n");CHECK(rejection_preserves(text,base));
    }
    for(const auto &tail:{"junk\n","end\n","shot\n"})CHECK(rejection_preserves(good+tail,base));
    auto truncated=good;truncated.erase(truncated.rfind("end"));CHECK(rejection_preserves(truncated,base));
    auto unknown=good;unknown.replace(unknown.find("shot"),4,"unknown");CHECK(rejection_preserves(unknown,base));
    auto noquotes=good;const auto quote=noquotes.find('"');noquotes.erase(quote);CHECK(rejection_preserves(noquotes,base));
    auto openquote=good;openquote.erase(openquote.rfind('"'));CHECK(rejection_preserves(openquote,base));
    auto nul=good;nul.insert(nul.find("shot"),1,'\0');CHECK(rejection_preserves(nul,base));
    CHECK(rejection_preserves(std::string(8192,' ')+good,base));
    CHECK(rejection_preserves(document(std::vector<story::HouseShot>(7,shot)),base));
    auto too_long=std::vector<story::HouseShot>(6,shot);for(auto &s:too_long)s.duration=8.;CHECK(rejection_preserves(document(too_long),base));
    for(double duration:{0.,.999,12.001,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
        auto invalid=shot;invalid.duration=duration;CHECK(rejection_preserves(document({invalid}),base));
    }
    for(const std::string &subtitle:std::vector<std::string>{std::string(241,'x'),"line one\nline two","tab\tcharacter",std::string("delete")+char(127)}){
        auto invalid=shot;invalid.subtitle=subtitle;CHECK(rejection_preserves(document({invalid}),base));
    }
    for(double fov:{.349,1.501,std::numeric_limits<double>::infinity()}){
        auto invalid=shot;invalid.rail.fov_from=fov;CHECK(rejection_preserves(document({invalid}),base));
    }
    {auto invalid=shot;invalid.rail.eye_from.x=1000001.;CHECK(rejection_preserves(document({invalid}),base));}
    {auto invalid=shot;invalid.rail.eye_from.x+=253.;CHECK(rejection_preserves(document({invalid}),base));}
    {auto invalid=shot;invalid.rail.target_from=invalid.rail.eye_from;CHECK(rejection_preserves(document({invalid}),base));}
    {auto invalid=shot;invalid.rail.eye_to.x+=.01;CHECK(rejection_preserves(document({invalid}),base));}
    {auto invalid=shot;invalid.rail.target_to.x+=.01;CHECK(rejection_preserves(document({invalid}),base));}
    {auto invalid=shot;invalid.rail.fov_to+=.01;CHECK(rejection_preserves(document({invalid}),base));}
    {auto invalid=base;invalid.meadow_goal.x+=1000.;CHECK(rejection_preserves(good,invalid));}
    // Byte-limit acceptance is exact and permits trailing whitespace only.
    std::istringstream exact(good+std::string(8192-good.size(),' '));CHECK(settings::parse(exact,base,out,error));
    CHECK(rejection_preserves(good+std::string(8193-good.size(),' '),base));
    std::istringstream broken(good);broken.setstate(std::ios::badbit);const auto before=document(out);
    CHECK(!settings::parse(broken,base,out,error));CHECK(!error.empty()&&document(out)==before);
    if(argc>1){
        // Optional production-asset check, using the exact same parser that
        // runtime uses. Waypoints do not affect cinematic validation.
        story::Config actual;std::ifstream config(std::string(argv[1])+"/opening.config");CHECK(bool(config));
        std::string line;unsigned found=0;
        while(std::getline(config,line)){
            std::istringstream row(line);row.imbue(std::locale::classic());std::string tag;row>>tag;
            auto vec=[&](story::Vec &v){return bool(row>>v.x>>v.y>>v.z);};
            auto pose=[&](story::Pose &p){return vec(p.feet)&&bool(row>>p.yaw>>p.pitch);};
            auto rail=[&](story::Rail &r){return vec(r.eye_from)&&vec(r.eye_to)&&vec(r.target_from)&&vec(r.target_to)&&bool(row>>r.fov_from>>r.fov_to);};
            if(tag=="house_spawn"){CHECK(pose(actual.house_spawn));found|=1;}
            else if(tag=="meadow_spawn"){CHECK(pose(actual.meadow_spawn));found|=2;}
            else if(tag=="house_focus"){CHECK(vec(actual.house_focus));found|=4;}
            else if(tag=="house_exit"){CHECK(vec(actual.house_exit));found|=8;}
            else if(tag=="meadow_goal"){CHECK(vec(actual.meadow_goal));found|=16;}
            else if(tag=="house_rail"){CHECK(rail(actual.house_rail));found|=32;}
            else if(tag=="transport_rail"){CHECK(rail(actual.transport_rail));found|=64;}
            else if(tag=="meadow_rail"){CHECK(rail(actual.meadow_rail));found|=128;}
        }
        CHECK(found==255);std::ifstream authored(std::string(argv[1])+"/opening/cinematic.settings");CHECK(bool(authored));
        CHECK(settings::parse(authored,actual,out,error));CHECK(!out.empty());
        std::cout<<"Production project parsed: "<<argv[1]<<" ("<<out.size()<<" shots)\n";
    }
    std::cout<<"opening cinematic settings: "<<checks<<" bounded parsing, version, text, geometry, duration, handoff and transactional rejection checks passed\n";
}

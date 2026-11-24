#include "engine/world_studio_scene_record.h"
#include <array>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

using world_studio_scene_record::Record;
static unsigned checks=0;
#define CHECK(x) do {++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';return false;}} while(0)
struct Parsed {std::string name;std::vector<double> numbers;};
static std::string encode(const Record &record){std::ostringstream stream;stream.imbue(std::locale::classic());world_studio_scene_record::write(stream,record);return stream.str();}
static Parsed decode(const std::string &text){
    Parsed out;std::istringstream stream(text);stream.imbue(std::locale::classic());stream>>out.name;double value=0;while(stream>>value)out.numbers.push_back(value);return out;
}
static double root(const Parsed &record,double terrain){return record.numbers.size()==13?record.numbers[12]:terrain-record.numbers[4]+record.numbers[5]+record.numbers[6];}
static bool close(double a,double b,double epsilon=1e-7){return std::abs(a-b)<=epsilon;}
static bool elevated_apartment(){
    // Actual v16/C2 source rows that disappeared when adding the first quest
    // forced Studio::scene to reconstruct the purchased apartment scene.
    // The engine captures their pivots as float WorldSceneryPlacement::y.
    struct Fixture {const char *name;double u,v,pivot;};
    static constexpr Fixture rows[]={
        {"nm_opening_home",.4652480158730159,.30605304711585762,602.59999942779541},
        {"nm_opening_reference_finward_scene_m1_0_a",.46519345238095239,.30592077198358247,599.99960087196087},
        {"nm_opening_reference_finward_scene_m1_0_b_a",.46524305554609452,.30599131872079588,599.99343186430633},
        {"nm_opening_reference_finward_scene_m1_0_b_b",.46524425024787586,.30592077198358247,599.99905515857972},
        {"nm_opening_reference_finward_scene_0_0_a",.46531249999526947,.30597368203649256,599.99902999878395},
        {"nm_opening_reference_finward_scene_0_0_b",.46538681795672765,.30597368203228764,599.99602267844602},
        {"nm_opening_reference_finward_scene_1_m1",.46551835317460316,.3060312413899316,599.9999999951873},
        {"nm_opening_reference_finward_scene_1_0_a_a",.46547292917493793,.30600895540509926,599.99947769165738},
        {"nm_opening_reference_finward_scene_1_0_a_b",.46547525269644602,.30595007900083315,599.99985282198759},
        {"nm_opening_reference_finward_scene_1_0_b_a",.46552827380952383,.30596486370275067,599.99810244748369},
        {"nm_opening_reference_finward_scene_1_0_b_b",.4655724878916665,.30598250037864422,599.99311727192253},
        {"nm_opening_reference_finward_scene_2_m1",.46563492063492062,.30603636192141825,599.98968710191548},
        {"nm_opening_reference_finward_scene_2_0",.46562003968253968,.30596486369854575,599.99291050387546},
        {"nm_opening_reference_home_story",.46527777777777779,.30602659208940258,600.50999997052895},
        {"nm_opening_reference_home_enclosure",.46527777777777779,.30602659208940258,599.84500000000003}
    };
    CHECK(std::size(rows)==15);
    std::ostringstream transaction;
    for(const auto &fixture:rows){
        Record r;r.name=fixture.name;r.u=fixture.u;r.v=fixture.v;r.pivot=float(fixture.pivot);r.preserve_source_root=true;
        const auto text=encode(r);const auto parsed=decode(text);
        CHECK(text.back()=='\n');CHECK(text.size()<=257);CHECK(parsed.name==fixture.name);CHECK(parsed.numbers.size()==13);
        CHECK(parsed.numbers[0]==fixture.u&&parsed.numbers[1]==fixture.v);
        CHECK(parsed.numbers[7]==0&&parsed.numbers[8]==0&&parsed.numbers[9]==1&&parsed.numbers[10]==1&&parsed.numbers[11]==1);
        CHECK(parsed.numbers[12]==double(r.pivot));CHECK(close(parsed.numbers[12],fixture.pivot,1e-4));
        // Source pivots stay at their authored elevation even when the land
        // beneath the isolated apartment has a very different normal height.
        for(double ground:{1.,22.,68.,150.}){CHECK(root(parsed,ground)==double(r.pivot));CHECK(root(parsed,ground)>599);}
        world_studio_scene_record::write(transaction,r);
    }
    std::istringstream lines(transaction.str());std::string line;unsigned count=0;
    while(std::getline(lines,line)){CHECK(decode(line).numbers.size()==13);++count;}CHECK(count==15);
    return true;
}
static bool grounding_policy(){
    Record source;source.name="unchanged_ground_source";source.u=.485;source.v=.32;source.pivot=24.75f;source.preserve_source_root=true;
    auto parsed=decode(encode(source));CHECK(parsed.numbers.size()==13);CHECK(root(parsed,30)==24.75);
    Record added=source;added.name="new_identity_prop";added.preserve_source_root=false;
    parsed=decode(encode(added));CHECK(parsed.numbers.size()==7);CHECK(root(parsed,30)==30);
    // New placements and identity source overrides keep the preexisting
    // terrain/offset policy. A previous source pivot is not transplanted when
    // the host marks an XZ/terrain edit as requiring grounding.
    added.u+=.001;added.v-=.002;added.burial=.25f;added.elevation=.5f;added.offset_y=1.25f;added.pivot=600;
    parsed=decode(encode(added));CHECK(parsed.numbers.size()==7);CHECK(close(root(parsed,27.125),28.625));CHECK(root(parsed,27.125)!=600);
    const auto same=encode(added);added.pivot=900;CHECK(encode(added)==same);
    // The same identity placement continues to follow a changed slope; the
    // writer does not force an absolute height onto normal terrain props.
    for(double terrain:{19.125,24.,31.5})CHECK(close(root(parsed,terrain),terrain+1.5));
    // An explicit graph/full transform retains the existing pivot behavior.
    for(unsigned component=0;component<5;++component){
        auto transformed=added;transformed.pivot=601.25f;
        if(component==0)transformed.pitch=12;
        if(component==1)transformed.roll=-7;
        if(component==2)transformed.sx=1.25f;
        if(component==3)transformed.sy=.75f;
        if(component==4)transformed.sz=1.5f;
        const auto text=encode(transformed);const auto full=decode(text);CHECK(full.numbers.size()==13);CHECK(root(full,27.125)==601.25);
        CHECK(full.numbers[7]==transformed.pitch&&full.numbers[8]==transformed.roll&&full.numbers[9]==transformed.sx&&full.numbers[10]==transformed.sy&&full.numbers[11]==transformed.sz);
        transformed.preserve_source_root=true;CHECK(encode(transformed)==text);
    }
    return true;
}
static bool bounded_fields(){
    for(float yaw:{-360.f,0.f,360.f})for(float scale:{.15f,1.f,4.f})for(float offset:{-128.f,0.f,128.f}){
        Record r;r.name="bounded_source";r.u=0;r.v=1;r.yaw=yaw;r.scale=scale;r.burial=2;r.elevation=3;r.offset_y=offset;
        r.pitch=-360;r.roll=360;r.sx=.1f;r.sy=8;r.sz=1.5f;r.pivot=12000;r.preserve_source_root=true;
        auto text=encode(r);auto p=decode(text);CHECK(text.size()<=257);CHECK(p.numbers.size()==13);
        const std::array<double,13> expected={r.u,r.v,r.yaw,r.scale,r.burial,r.elevation,r.offset_y,r.pitch,r.roll,r.sx,r.sy,r.sz,r.pivot};
        for(unsigned i=0;i<expected.size();++i){CHECK(std::isfinite(p.numbers[i]));CHECK(p.numbers[i]==expected[i]);}
        r.pivot=-2000;p=decode(encode(r));CHECK(p.numbers[12]==-2000);
    }
    // A non-finite fallback is never serialized as an explicit pivot. Normal
    // fields keep their prior compact shape; transformed ones keep all5 TRS
    // values even when the native loader will determine the root from ground.
    for(float pivot:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
        Record r;r.name="missing_pivot";r.u=.5;r.v=.5;r.pivot=pivot;r.preserve_source_root=true;
        auto text=encode(r);auto p=decode(text);CHECK(p.numbers.size()==7);CHECK(text.find("nan")==std::string::npos&&text.find("inf")==std::string::npos);
        r.roll=17;text=encode(r);p=decode(text);CHECK(p.numbers.size()==12);CHECK(text.find("nan")==std::string::npos&&text.find("inf")==std::string::npos);CHECK(p.numbers[8]==17);
    }
    return true;
}
int main(){if(!elevated_apartment()||!grounding_policy()||!bounded_fields())return 1;std::cout<<"world_studio_scene_record: "<<checks<<" checks;15 actual elevated identity source rows, absolute pivot preservation, normal terrain/edited-XZ grounding, offsets, slopes and full transforms\n";}

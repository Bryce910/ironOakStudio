#include "engine/world_playtest_slice.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
using namespace world_playtest_slice;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
static Path sample(){
    Path p;p.name="A \"quoted\" path / forêt";p.spawn={-10,3.75,7,.8,-.2};
    p.nodes={{"Entry",0,2,0,1,true},{"Hidden connector",10,3,0,1,false},{"Return",0,2,0,1,true}};
    return p;
}
static std::string encoded(const Path &p){std::string s,e;CHECK(encode(p,s,e));CHECK(e.empty());return s;}
static void invalid(const std::string &s){
    Path target=sample();const auto before=encoded(target);std::string error;
    std::istringstream input(s);CHECK(!parse(input,target,error));CHECK(!error.empty());CHECK(encoded(target)==before);
}
static void codec(){
    auto p=sample();auto s=encoded(p);Path decoded;std::string error;
    std::istringstream input(s);CHECK(parse(input,decoded,error));CHECK(encoded(decoded)==s);
    CHECK(decoded.nodes[0].visible&&!decoded.nodes[1].visible);CHECK(decoded.spawn.eye_y==3.75);
    s.pop_back();std::istringstream eof_input(s);CHECK(parse(eof_input,decoded,error));
    std::string crlf;for(char c:s){if(c=='\n')crlf+='\r';crlf+=c;}
    std::istringstream cr_input("\n"+crlf+"\n\n");CHECK(parse(cr_input,decoded,error));
    const std::string base="VALWOOD_PLAYTEST 1\nname \"Route\"\nspawn 0 1.75 0 0 0\ncount 1\n";
    const std::string point="path \"Gate\" 0 0 0 1 checkpoint\n";
    invalid("");invalid("VALWOOD_PLAYTEST 2\n");invalid(base);invalid(base+point+point);
    invalid(base+"path Gate 0 0 0 1\n");invalid(base+"path \"Gate\" 0 0 0 1 typo\n");
    invalid(base+"path \"Gate\" 0 0 0 1 checkpoint extra\n");
    invalid(base+"path \"Gate\" nan 0 0 1\n");invalid(base+"path \"Gate\" 0 0 inf 1\n");
    invalid(base+"path \"Gate\" 0 0 0 0\n");invalid(base+"path \"Gate\" 1000001 0 0 1\n");
    invalid(base+"path \"\" 0 0 0 1 checkpoint\n");
    invalid("VALWOOD_PLAYTEST 1\nname Route\nspawn 0 1.75 0 0 0\ncount 1\n"+point);
    invalid("VALWOOD_PLAYTEST 1 extra\nname \"Route\"\nspawn 0 1.75 0 0 0\ncount 1\n"+point);
    invalid("VALWOOD_PLAYTEST 1\nname \"Route\"\nspawn 0 1.75 0 0 0 extra\ncount 1\n"+point);
    invalid("VALWOOD_PLAYTEST 1\nname \"Route\"\nspawn 0 1.75 0 0 0\ncount -1\n"+point);
    invalid("VALWOOD_PLAYTEST 1\nname \"Route\"\nspawn 0 1.75 0 0 0\ncount 257\n"+point);
    invalid(base+"path \"unterminated 0 0 0 1\n");
    invalid(base+"path \"Control\tname\" 0 0 0 1\n");
    invalid(base+std::string(1025,' '));invalid(std::string(byte_limit+1,'\n'));
    invalid(base+point+std::string(1,'\0'));
    std::istringstream failed(s);failed.setstate(std::ios::failbit);
    const auto preserved=encoded(decoded);CHECK(!parse(failed,decoded,error));CHECK(encoded(decoded)==preserved);
    p.nodes.assign(node_limit,p.nodes[0]);const auto maximum=encoded(p);std::istringstream max_input(maximum);
    CHECK(parse(max_input,decoded,error));CHECK(decoded.nodes.size()==node_limit);
    p.nodes.push_back(p.nodes[0]);std::string unchanged="keep";CHECK(!encode(p,unchanged,error));CHECK(unchanged=="keep");
    p=sample();p.spawn.pitch=2.;CHECK(!validate(p,error));p=sample();p.name.assign(text_limit+1,'x');CHECK(!validate(p,error));
    p=sample();p.nodes[0].ground_y=std::numeric_limits<double>::infinity();CHECK(!validate(p,error));
    CHECK(!load("/does/not/exist/playtest.route",decoded,error));CHECK(!load(nullptr,decoded,error));
}
static void progress(){
    State state;CHECK(!state.active()&&!state.complete()&&state.next()==nullptr);
    auto p=sample();std::string error;CHECK(state.start(p,&error));CHECK(error.empty());
    CHECK(state.active()&&state.index()==0&&state.reached()==0&&state.total()==2);
    p.nodes.clear();p.name="Changed externally";CHECK(state.path().nodes.size()==3);
    CHECK(std::string(state.getlabel())=="Entry");CHECK(state.distance(3,4)==5.);
    CHECK(!state.update(10,3,0)); // Cannot skip to a later waypoint.
    CHECK(!state.update(0,4.00001,0)); // Wrong floor, despite horizontal overlap.
    CHECK(!state.update(std::numeric_limits<double>::quiet_NaN(),2,0));CHECK(state.index()==0);
    CHECK(!state.update(1.00001,2,0));CHECK(state.update(1,4,0)); // Inclusive radius and height boundaries.
    CHECK(state.index()==1&&state.reached()==1&&!state.complete());
    CHECK(!state.update(0,2,0)); // Closed-loop final point cannot be reached out of order.
    CHECK(state.update(10,3,0));CHECK(state.index()==2&&state.reached()==1);
    CHECK(state.update(0,2,0));CHECK(state.complete()&&state.reached()==2&&state.next()==nullptr);
    CHECK(!state.update(0,2,0));CHECK(state.distance(0,0)==0.);
    state.reset();CHECK(state.index()==0&&state.reached()==0&&!state.complete()&&state.total()==2);
    Path bad;CHECK(!state.start(bad,&error));CHECK(state.path().nodes.size()==3&&state.index()==0);
    auto stacked=sample();stacked.nodes[1]=stacked.nodes[0];CHECK(state.start(stacked));
    CHECK(state.update(0,2,0));CHECK(state.complete()&&state.reached()==3); // Bounded coincident-node consumption.
}
static void sessions(){
    auto p=sample();State state;std::string error;CHECK(state.start(p));
    Session saved;saved.section=1;saved.fingerprint=fingerprint(p);saved.index=2;
    saved.x=-13.125;saved.eye_y=22.75;saved.z=10;saved.yaw=.5;saved.pitch=-.1;saved.third_person=true;
    std::string bytes;CHECK(encode_session(saved,bytes,error));Session parsed;
    std::istringstream input(bytes);CHECK(parse_session(input,parsed,error));
    CHECK(parsed.section==1&&parsed.fingerprint==saved.fingerprint&&parsed.index==2);
    CHECK(parsed.x==saved.x&&parsed.eye_y==saved.eye_y&&parsed.third_person);
    CHECK(restore_session(parsed,state,error));CHECK(state.index()==2&&state.reached()==1);
    auto corrupt=[&](const std::string &text){
        Session output=saved;std::istringstream stream(text);CHECK(!parse_session(stream,output,error));CHECK(!error.empty());
        std::string after;CHECK(encode_session(output,after,error));CHECK(after==bytes);
    };
    corrupt(bytes+"section 0\n");corrupt(bytes+"player 1 2 3 0 0 0\n");corrupt(bytes.substr(0,bytes.find("player")));
    corrupt("VALWOOD_PLAYTEST_SESSION 2\n");corrupt(std::string(2049,' '));corrupt(bytes+std::string(1,'\0'));
    const std::string head="VALWOOD_PLAYTEST_SESSION 1\nsection 0\nfingerprint 1\nindex 0\n";
    corrupt(head+"player nan 1 0 0 0 0\n");corrupt(head+"player 0 inf 0 0 0 0\n");
    corrupt(head+"player 1000001 1 0 0 0 0\n");corrupt(head+"player 0 100001 0 0 0 0\n");
    corrupt(head+"player 0 1 0 0 2 0\n");corrupt(head+"player 0 1 0 100001 0 0\n");
    corrupt(head+"player 0 1 0 0 0 2\n");
    corrupt("VALWOOD_PLAYTEST_SESSION 1\nsection 2\nfingerprint 1\nindex 0\nplayer 0 1 0 0 0 0\n");
    corrupt("VALWOOD_PLAYTEST_SESSION 1\nsection 0\nfingerprint -1\nindex 0\nplayer 0 1 0 0 0 0\n");
    corrupt("VALWOOD_PLAYTEST_SESSION 1\nsection 0\nfingerprint 18446744073709551616\nindex 0\nplayer 0 1 0 0 0 0\n");
    corrupt("VALWOOD_PLAYTEST_SESSION 1\nsection 0\nfingerprint 0\nindex 0\nplayer 0 1 0 0 0 0\n");
    corrupt("VALWOOD_PLAYTEST_SESSION 1\nsection 0\nfingerprint 1\nindex 257\nplayer 0 1 0 0 0 0\n");
    auto stale=saved;stale.fingerprint^=17;CHECK(!restore_session(stale,state,error));CHECK(state.index()==2&&state.reached()==1);
    auto changed=p;changed.nodes[0].ground_y+=.01;CHECK(fingerprint(changed)!=fingerprint(p));
    std::istringstream route_input(encoded(p));Path roundtrip;CHECK(parse(route_input,roundtrip,error));CHECK(fingerprint(roundtrip)==fingerprint(p));
    stale=saved;stale.index=4;CHECK(!restore_session(stale,state,error));CHECK(state.index()==2);
    CHECK(!state.restore_index(4,error));CHECK(state.index()==2);
    CHECK(state.restore_index(3,error));CHECK(state.complete()&&state.reached()==2);
    CHECK(state.restore_index(0,error));CHECK(state.index()==0&&state.reached()==0);
    State empty;CHECK(!restore_session(saved,empty,error));CHECK(!empty.restore_index(0,error));
    saved.x=std::numeric_limits<double>::infinity();std::string unchanged="keep";
    CHECK(!encode_session(saved,unchanged,error));CHECK(unchanged=="keep");
    Path bad;CHECK(fingerprint(bad)==0);
}
static void fixture(const char *filename){
    Path p;std::string error;CHECK(load(filename,p,error));CHECK(!p.nodes.empty());
    const auto bytes=encoded(p);Path q;std::istringstream in(bytes);CHECK(parse(in,q,error));CHECK(encoded(q)==bytes);
    State s;CHECK(s.start(q));
    // Feed each waypoint at its actual supporting height. Height rejection is
    // checked first; then the complete ordered path must be consumable.
    const auto first=q.nodes.front();CHECK(!s.update(first.x,first.ground_y+2.001,first.z));
    for(const auto &n:q.nodes)s.update(n.x,n.ground_y,n.z);
    CHECK(s.complete());CHECK(s.reached()==s.total());CHECK(s.total()>=3&&s.total()<=12);
    s.reset();CHECK(!s.complete()&&s.index()==0&&s.reached()==0);
    std::cout<<"ROUTE_PASS "<<p.nodes.size()<<" nodes "<<s.total()<<" milestones "<<p.name<<'\n';
}
int main(int argc,char **argv){
    if(argc==1){codec();progress();sessions();}
    else if(argc==2)fixture(argv[1]);else return 2;
    std::cout<<"PLAYTEST_SLICE_PASS "<<checks<<" checks\n";
}

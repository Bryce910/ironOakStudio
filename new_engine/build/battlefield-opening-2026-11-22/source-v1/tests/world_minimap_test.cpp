#include "engine/world_minimap.h"
#include <cassert>
#include <initializer_list>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <limits>
namespace mm=world_minimap;
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::abort();}}while(false)
const std::string base="VALWOOD_MINIMAP 1\nenabled 1\nradius 140\nregion -12288 4992 -11008 5760\n";
bool parse(const std::string &text,mm::Config &c){std::istringstream input(text);std::string error;return mm::parse(input,c,error);}
double cross(mm::Point a,mm::Point b,mm::Point c){return (b.x-a.x)*(c.z-a.z)-(b.z-a.z)*(c.x-a.x);}
bool intersects(mm::Point a,mm::Point b,mm::Point c,mm::Point d){return cross(a,b,c)*cross(a,b,d)<=0&&cross(c,d,a)*cross(c,d,b)<=0;}
int main(int argc,char **argv){
    mm::Config config;
    CHECK(parse(base+"landmark camp -11988 5172 \"CAMP\"\nlandmark town -11968 5582 \"TOWN\"\n",config));
    CHECK(config.enabled&&config.radius==140&&config.count==2);
    // Invalid input never partially replaces a previously accepted map.
    for(const std::string &text:std::initializer_list<std::string>{"", "VALWOOD_MINIMAP 2\nenabled 1\nradius 140\nregion -12288 4992 -11008 5760\n",base+"enabled 0\n",base+"radius 160\n",base+"anything 1\n",base+"landmark quest -11988 5172 \"CAMP\"\n",base+"landmark camp -11988 5172 CAMP\n",base+"landmark camp 0 0 \"OUTSIDE\"\n",base+"landmark camp -11988 5172 \"ABCDEFGHIJKLMNOPQRSTUVWXYZ\"\n",base+"landmark camp -11988 5172 \"CAMP\" extra\n",base+"landmark camp -11988 5172 \"CAMP\"\nlandmark town -11968 5582 \"CAMP\"\n",std::string(5000,'#')}){
        const auto previous=config;CHECK(!parse(text,config));CHECK(config.enabled==previous.enabled&&config.radius==previous.radius&&config.count==previous.count);
    }
    for(const char *radius:{"0","79.9","320.1","nan","inf","-140"}){std::string text=base;text.replace(text.find("radius 140"),10,std::string("radius ")+radius);CHECK(!parse(text,config));}
    for(const char *region:{"region -12288 4992 -12280 5760","region -12288 5760 -11008 4992","region -999999 4992 -11008 5760","region -12288 nan -11008 5760"}){auto text=base;text.replace(text.find("region"),text.size()-text.find("region"),std::string(region)+"\n");CHECK(!parse(text,config));}
    std::string many=base;for(unsigned i=0;i<9;++i)many+="landmark camp -11988 5172 \"CAMP"+std::to_string(i)+"\"\n";CHECK(!parse(many,config));
    mm::Point q;const mm::Point player{-11988,5180};
    CHECK(mm::project(player,player,140,q)&&q.x==0&&q.z==0);
    CHECK(mm::project({player.x+140,player.z},player,140,q)&&q.x==1&&q.z==0);
    CHECK(mm::project({player.x,player.z-140},player,140,q)&&q.x==0&&q.z==-1);
    CHECK(mm::project({player.x,player.z+70},player,140,q)&&q.z==.5);
    const auto north=mm::heading(0),east=mm::heading(mm::pi/2),south=mm::heading(mm::pi),west=mm::heading(-mm::pi/2);
    CHECK(north.x==0&&north.z==-1);CHECK(std::abs(east.x-1)<1.e-12&&std::abs(east.z)<1.e-12);
    CHECK(std::abs(south.x)<1.e-12&&std::abs(south.z-1)<1.e-12);CHECK(std::abs(west.x+1)<1.e-12&&std::abs(west.z)<1.e-12);
    CHECK(!mm::project(player,player,0,q)&&!mm::project({NAN,0},player,140,q));
    mm::Point a{-4,0},b{4,0};CHECK(mm::clip(a,b)&&a.x==-1&&b.x==1);
    a={0,-4};b={0,4};CHECK(mm::clip(a,b)&&a.z==-1&&b.z==1);
    a={-4,2};b={4,2};CHECK(!mm::clip(a,b));
    a={-4,1.1};b={4,1.1};CHECK(mm::clip(a,b,.2)&&std::abs(a.x+1.2)<1.e-12);
    a={NAN,0};b={1,1};CHECK(!mm::clip(a,b));
    // Independent segment/rectangle intersection reference, then ensure every
    // clipped endpoint is inside and lies on the original world segment.
    unsigned accepted=0;
    for(unsigned i=0;i<10000;++i){
        mm::Point start{double((i*73)%997)/100-5,double((i*37+11)%991)/100-5};
        mm::Point end{double((i*43+7)%983)/100-5,double((i*89+19)%977)/100-5};
        const bool reference=mm::inside(start)||mm::inside(end)||intersects(start,end,{-1,-1},{1,-1})||intersects(start,end,{1,-1},{1,1})||intersects(start,end,{1,1},{-1,1})||intersects(start,end,{-1,1},{-1,-1});
        a=start;b=end;const bool actual=mm::clip(a,b);CHECK(actual==reference);
        if(actual){++accepted;CHECK(mm::inside(a,1.000000001)&&mm::inside(b,1.000000001));CHECK(std::abs(cross(start,end,a))<1.e-10&&std::abs(cross(start,end,b))<1.e-10);}
    }
    CHECK(accepted>1000);
    q=mm::clamp_marker({2,5});CHECK(std::abs(q.z-.86)<1.e-12&&std::abs(q.x-.344)<1.e-12);
    q=mm::clamp_marker({.1,.2});CHECK(q.x==.1&&q.z==.2);
    mm::Visibility visible;
    CHECK(mm::visible(config,player,visible));
    visible.hud=false;CHECK(!mm::visible(config,player,visible));visible={};visible.editor=true;CHECK(!mm::visible(config,player,visible));visible={};visible.modal=true;CHECK(!mm::visible(config,player,visible));visible={};visible.dialogue=true;CHECK(!mm::visible(config,player,visible));visible={};visible.atlas=true;CHECK(!mm::visible(config,player,visible));visible={};visible.graphics=true;CHECK(!mm::visible(config,player,visible));visible={};visible.loading=true;CHECK(!mm::visible(config,player,visible));visible={};visible.cinematic=true;CHECK(!mm::visible(config,player,visible));visible={};visible.flying=true;CHECK(!mm::visible(config,player,visible));visible={};CHECK(!mm::visible(config,{0,0},visible));config.enabled=false;CHECK(!mm::visible(config,player,visible));
    mm::Geometry geometry;for(unsigned i=0;i<mm::line_limit;++i)CHECK(geometry.append({double(i),0},{double(i+1),1},4,mm::LineKind::Road));CHECK(!geometry.append({0,0},{1,1},4,mm::LineKind::Road)&&geometry.count==mm::line_limit&&geometry.truncated==1);geometry.clear();CHECK(geometry.count==0&&geometry.truncated==0);CHECK(!geometry.append({NAN,0},{1,1},4,mm::LineKind::River));
    mm::Budget budget;CHECK(budget.take(mm::primitive_limit)&&!budget.take(1)&&budget.used==mm::primitive_limit);
    if(argc>1){std::ifstream actual(argv[1]);std::string error;CHECK(actual&&mm::parse(actual,config,error));CHECK(config.enabled&&config.radius==140&&config.count==4);}
    std::printf("PASS minimap CPU: %u checks; config atomic validation, real config, north-up/facing/projection,10k independent rectangle intersections, context visibility and fixed geometry/primitive budgets\n",checks);
}

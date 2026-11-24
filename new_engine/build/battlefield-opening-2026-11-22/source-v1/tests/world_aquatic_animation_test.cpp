#include "engine/world_aquatic_animation.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace world_aquatic_animation;
static unsigned checks=0;
static void require(bool b,const char *m){++checks;if(!b){std::fprintf(stderr,"FAIL %s\n",m);std::exit(1);}}
int main(){
 State s;auto a=s.update(false,false,false,.016);require(!a.active,"dry player has no aquatic override");
 a=s.update(true,false,false,.016);require(a.active&&a.mode==Mode::Tread&&!a.transition,"stationary surface player treads");
 auto frozen=a;s.update(true,false,false,10,true);require(s.value.from==frozen.from&&s.value.weight==frozen.weight,"pause freezes committed pose");
 a=s.update(true,false,true,.016);require(a.mode==Mode::Stroke&&a.transition&&a.weight<.01f,"movement starts bounded pose handoff");
 float last=a.weight;for(int i=0;i<24;++i){a=s.update(true,false,true,.016);if(a.transition){require(a.weight>=last,"handoff blend progresses monotonically");last=a.weight;}}
 require(a.mode==Mode::Stroke,"surface movement never selects gait");
 bool wrap=false;unsigned old=0;
 for(int i=0;i<500;++i){a=s.update(true,false,true,.016);require(a.active&&a.from>=16&&a.from<32&&a.to>=16&&a.to<32,"stroke stays in dedicated cyclic bank");require(std::isfinite(a.weight)&&a.weight>=0&&a.weight<=1,"interpolation remains valid");if(old==31&&a.from==16)wrap=true;old=a.from;}
 require(wrap,"last stroke sample wraps directly to its first target");
 a=s.update(true,true,true,.016);require(a.mode==Mode::Dive&&a.transition,"actual underwater state chooses dedicated dive loop");
 for(int i=0;i<30;++i){a=s.update(true,true,false,.016);}require(a.from>=32&&a.to>=32,"underwater stationary state retains underwater clip");
 a=s.update(true,false,false,.016);require(a.mode==Mode::Tread&&a.transition,"underwater stop eases into surface treading");
 State swimming;
 for(int i=0;i<100;++i)swimming.update(true,false,true,.016);
 const auto before_pause=swimming.value;
 auto during_pause=swimming.update(true,false,false,.016);
 require(during_pause.mode==Mode::Stroke&&!during_pause.transition,"brief zero-travel gap retains swim loop");
 require(during_pause.phase!=0&&during_pause.phase>=before_pause.phase,"brief input gap preserves playhead");
 for(int i=0;i<10;++i)during_pause=swimming.update(true,false,false,.016);
 require(during_pause.mode==Mode::Stroke,"controller neutral gap does not immediately flip upright");
 for(int i=0;i<6;++i)during_pause=swimming.update(true,false,false,.016);
 require(during_pause.mode==Mode::Tread&&during_pause.transition,"sustained stop eases into tread");
 require(during_pause.to%frames==during_pause.from%frames,"mode handoff preserves stroke sample instead of restarting frame zero");
 for(int i=0;i<40;++i)a=s.update(true,false,false,.016);
 require(a.mode==Mode::Tread,"sustained surface stop settles into relaxed treading");
 frozen=a;a=s.update(true,false,false,std::numeric_limits<double>::quiet_NaN());require(a.from==frozen.from&&a.weight==frozen.weight,"invalid time cannot mutate pose state");
 a=s.update(false,false,false,.016);require(!a.active&&!s.initialized,"dry exit clears dedicated animation and transition ownership");
 std::printf("world_aquatic_animation_test: PASS %u checks\n",checks);
}

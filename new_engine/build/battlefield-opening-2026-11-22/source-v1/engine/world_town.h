#ifndef VALWOOD_WORLD_TOWN_H
#define VALWOOD_WORLD_TOWN_H
#include <algorithm>
#include <cmath>
#include <string>
// Small persistent player inventory. Quest rewards remain authoritative in the
// gameplay runtime; spent coin is subtracted from earned coin, never minted here.
namespace world_town {
struct State {int spent=0,bread=0,apples=0,prepared=0,welcome=0;};
inline bool valid(const State &s){return s.spent>=0&&s.spent<=1000000&&s.bread>=0&&s.bread<=8&&s.apples>=0&&s.apples<=8&&s.prepared>=0&&s.prepared<=7&&(s.welcome==0||s.welcome==1);}
inline int purse(const State &s,int earned){return std::max(0,earned-s.spent);}
enum class Action {Welcome,Bread,Apples,Sharpen,Meal,Rest,EatBread,EatApple};
inline bool transact(State &state,Action action,int earned,std::string &message){
 if(!valid(state)||earned<0||earned>1000000||state.spent>earned){message="This purse could not be verified. Restore your checkpoint.";return false;}
 State next=state;int cost=0;
 switch(action){
 case Action::Welcome:if(next.welcome){message="You have already collected your welcome ration.";return false;}if(next.bread==8){message="Your bread pouch is full (8).";return false;}next.welcome=1;++next.bread;message="A welcome loaf, on the house. Eat it from your journal before a trial.";break;
 case Action::Bread:if(next.bread==8){message="Your bread pouch is full (8).";return false;}cost=3;++next.bread;message="Bread packed. Eat it from your journal to prepare for your next encounter.";break;
 case Action::Apples:if(next.apples==8){message="Your fruit pouch is full (8).";return false;}cost=2;++next.apples;message="Orchard fruit packed. Eat it from your journal for faster stamina recovery.";break;
 case Action::Sharpen:case Action::Meal:case Action::Rest:{int flag=action==Action::Sharpen?1:action==Action::Meal?2:4;
  if(next.prepared&flag){message="You are already prepared. This benefit lasts for your next encounter.";return false;}
  next.prepared|=flag;cost=action==Action::Sharpen?6:action==Action::Meal?4:5;
  message=action==Action::Sharpen?"Blade honed: +15% strike damage for the next encounter.":action==Action::Meal?"A hot supper: 10% less incoming damage for the next encounter.":"Rested: +25% stamina recovery for the next encounter. Your checkpoint is saved.";break;}
 case Action::EatBread:case Action::EatApple:{int &count=action==Action::EatBread?next.bread:next.apples;int flag=action==Action::EatBread?2:4;
  if(!count){message="Your pouch is empty.";return false;}if(next.prepared&flag){message="That benefit is already prepared for your next encounter.";return false;}--count;next.prepared|=flag;
  message=action==Action::EatBread?"Well fed: 10% less incoming damage in your next encounter.":"Refreshed: +25% stamina recovery in your next encounter.";break;}
 }
 if(purse(state,earned)<cost){message="Not enough coin. Complete Corvin's journey to earn your watch pay.";return false;}
 next.spent+=cost;if(!valid(next)){message="Your pouch is full.";return false;}state=next;return true;
}
struct Benefits {float damage=1,resistance=1,recovery=1;};
inline Benefits consume(State &s){Benefits b;if(s.prepared&1)b.damage=1.15f;if(s.prepared&2)b.resistance=.9f;if(s.prepared&4)b.recovery=1.25f;s.prepared=0;return b;}
struct Venue {const char *name,*keeper,*greeting;double x,z,entry_x,entry_z;};
inline constexpr Venue venues[]={
 {"WATCH ARMORY","Bram / Watch armorer","Keep your edge true. I can hone your blade before the next trial.",-1357.4,-10326.8,-1357.4,-10326.8},
 {"KEEP PROVISIONS","Mara / Grocer","Bread from the morning ovens, fruit from the orchard. Your first loaf is on the house.",-1297,-10305.3,-1297,-10305.3},
 {"THE WATCHMAN'S REST","Elsie / Innkeeper","Come in from the yard. There is a hot meal, a quiet bed, and news by the fire.",-1282,-10290,-1293.5,-10287},
 {"THE COPPER LANTERN","Hugh / Innkeeper","The watch always has a seat here. Supper and a room, if you have coin.",-1457.4,-10290.6,-1455.4,-10280}
};
inline int nearby(double x,double y,double z,bool flying=false){if(flying||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||std::abs(y-24)>1)return -1;for(unsigned i=0;i<4;i++)if(std::hypot(x-venues[i].x,z-venues[i].z)<3.2)return int(i);return -1;}
}
#endif

#ifndef VALWOOD_WORLD_COMBAT_FATALITY_H
#define VALWOOD_WORLD_COMBAT_FATALITY_H
#include "world_combat_capacity.h"
#include <array>
#include <algorithm>
#include <cmath>
// Bounded presentation physics. Simulation never changes health, kill counts,
// reward receipts or the player's camera. One detached head per stable enemy ID.
namespace world_combat_fatality {
constexpr unsigned capacity=world_combat_capacity::npcs;
struct Head {bool active=false,settled=false;double x=0,y=0,z=0,vx=0,vy=0,vz=0,floor=0,age=0,spin=0,yaw=0,radius=.23;unsigned bounces=0;};
struct System {
 std::array<Head,capacity> heads={};std::array<bool,capacity> severed={};unsigned launches=0,peak=0;
 void reset(){heads={};severed={};launches=peak=0;}
 bool launch(unsigned id,double x,double y,double z,double dx,double dz,double floor,double radius,double yaw){
  if(id>=capacity||severed[id]||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(dx)||!std::isfinite(dz)||!std::isfinite(floor)||!std::isfinite(radius)||!std::isfinite(yaw)||radius<.1||radius>.4)return false;
  const double n=std::hypot(dx,dz);if(n<1e-6)return false;
  auto &h=heads[id];h={};h.active=true;h.x=x;h.y=std::max(y,floor+radius);h.z=z;h.floor=floor;h.radius=radius;h.yaw=yaw;h.vx=dx/n*2.4;h.vz=dz/n*2.4;h.vy=3.3;severed[id]=true;++launches;peak=std::max(peak,active());return true;
 }
 unsigned active()const{unsigned n=0;for(const auto &h:heads)n+=h.active;return n;}
 void update(double dt,bool paused=false){
  if(paused||!std::isfinite(dt)||dt<=0)return;dt=std::min(dt,.1);
  for(auto &h:heads)if(h.active){h.age+=dt;if(h.age>=60){h.active=false;continue;}if(h.settled)continue;
   double remaining=dt;while(remaining>1e-9){const double step=std::min(remaining,1./120);remaining-=step;
    h.vy-=9.81*step;h.x+=h.vx*step;h.y+=h.vy*step;h.z+=h.vz*step;h.spin+=step*2.1;h.yaw+=step*2.6;
    if(h.y<h.floor+h.radius){h.y=h.floor+h.radius;
     if(h.vy<0){h.vy=-h.vy*.27;h.vx*=.48;h.vz*=.48;++h.bounces;}
     if(h.vy<.32||h.bounces>=4){h.vx=h.vy=h.vz=0;h.settled=true;break;}
    }
   }
  }
 }
};
}
#endif

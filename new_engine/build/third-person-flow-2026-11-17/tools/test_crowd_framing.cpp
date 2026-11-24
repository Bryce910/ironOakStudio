#include "engine/world_combat_closeup_camera.h"
#include <iostream>
#include <stdexcept>
unsigned checks=0;void ck(bool p){++checks;if(!p)throw std::runtime_error("Crowd framing invariant");}
int main(){try{for(unsigned hz:{30u,60u,120u}){WorldCombatCloseupCamera one,two,three;
 for(unsigned n=0;n<hz*4;++n){float dt=1.f/hz;auto a=one.update(1,dt,true,n==0,1),b=two.update(1,dt,true,n==0,2),c=three.update(1,dt,true,n==0,3);
 ck(a.boom<=b.boom&&b.boom<=c.boom&&c.boom<=4.2f);ck(a.shoulder==b.shoulder&&b.shoulder==c.shoulder);ck(c.lift<=.5f);ck(a.target_drop==c.target_drop);}
 auto before=three.update(1,1.f/hz,true,false,3);auto next=three.update(1,1.f/hz,true,false,1);ck(before.boom-next.boom<.12f);
 auto off=three.update(1,1.f/hz,false,false,3);ck(off.boom==4.2f&&off.weight==0);}
 std::cout<<"PASS "<<checks<<" crowd framing checks at 30/60/120 Hz\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

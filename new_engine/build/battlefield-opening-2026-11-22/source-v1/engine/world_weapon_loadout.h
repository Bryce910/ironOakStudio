#ifndef VALWOOD_WORLD_WEAPON_LOADOUT_H
#define VALWOOD_WORLD_WEAPON_LOADOUT_H
#include <array>
namespace world_weapon_loadout {
// Keep the original sword/bow values stable for existing input and QA routes.
enum class Weapon {Sword,Bow,SwordShield,GreatAxe,Staff};
struct Profile {const char *name,*description;float damage,stamina;double reach;float speed,block_cost;bool edged;};
inline constexpr std::array<Profile,5> profiles={{
 {"Two-handed sword","Balanced cuts, thrusts and three-hit combos",1,1,2.35,1,22,true},
 {"Bow","Hold and release RT / left mouse to shoot",1,1,2.35,1,22,false},
 {"Sword & shield","One-handed cuts; shield blocks use less stamina",.9f,.85f,2.20,.90f,12,true},
 {"Two-handed axe","Heavy cuts; stronger impact and longer recovery",1.65f,1.35f,2.50,1.30f,26,true},
 {"Quarterstaff","Fast blunt strikes; long reach and economical combos",.80f,.70f,2.80,.82f,17,false}
}};
inline bool valid(Weapon w){return unsigned(w)<profiles.size();}
inline const Profile &profile(Weapon w){return profiles[valid(w)?unsigned(w):0];}
inline Weapon cycle(Weapon w,int direction){return Weapon((int(valid(w)?unsigned(w):0)+int(profiles.size())+(direction<0?-1:1))%int(profiles.size()));}
inline Weapon melee_cycle(Weapon w,int direction){auto next=cycle(w,direction);if(next==Weapon::Bow)next=cycle(next,direction);return next;}
}
#endif

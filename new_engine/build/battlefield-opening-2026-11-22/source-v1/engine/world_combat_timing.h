#ifndef VALWOOD_WORLD_COMBAT_TIMING_H
#define VALWOOD_WORLD_COMBAT_TIMING_H
#include "world_weapon_loadout.h"
#include "world_combat_difficulty.h"
#include <algorithm>
#include <cmath>

// Shared physical durations. Commit once per attack serial; animation and
// anatomical queries consume the same values in every camera view.
namespace world_combat_timing {
struct Durations {double windup=.18,strike=.09,recovery=.24,link=.08;};
// NPC source attacks have a readable release, independent of the player's
// equipped weapon. Direction ordinals are Left/Right/High/Low/Thrust. Reuse
// the committed windup as the tempo latch: changing difficulty while a weapon
// is already moving must not retime that swing or its geometric forecast.
inline double incoming_base_windup(unsigned round){return std::max(.50,.72-double(round)*.10);}
inline double incoming_strike(unsigned direction,double committed_windup_seconds,unsigned round=0){
    const double ratio=std::isfinite(committed_windup_seconds)&&committed_windup_seconds>0?committed_windup_seconds/incoming_base_windup(round):1.;
    const double source=direction==2?.29:direction==3?.27:direction==4?.22:.25;
    const double minimum=direction==2?.23:direction==3?.21:direction==4?.18:.20;
    return std::max(minimum,source*ratio);
}
inline double incoming_recovery(double committed_windup_seconds,unsigned round=0){
    const double ratio=std::isfinite(committed_windup_seconds)&&committed_windup_seconds>0?committed_windup_seconds/incoming_base_windup(round):1.;
    return std::max(.40,.50-double(round)*.05)*ratio;
}
inline Durations incoming(unsigned direction,world_combat_difficulty::Difficulty difficulty,unsigned round=0){
    const double windup=incoming_base_windup(round)/world_combat_difficulty::profile(difficulty).attack_tempo_scale;
    return {windup,incoming_strike(direction,windup,round),incoming_recovery(windup,round),0};
}
inline bool flowing_sword(world_weapon_loadout::Weapon weapon,unsigned direction){
    return weapon==world_weapon_loadout::Weapon::Sword&&(direction==0||direction==1||direction==4);
}
inline Durations attack(world_weapon_loadout::Weapon weapon,unsigned direction,bool chain=false,double sword_high_min=0){
    const double speed=world_weapon_loadout::profile(weapon).speed;
    Durations q{(chain?.14:direction==4?.17:.18)*speed,.09,.24*speed,.08};
    if(flowing_sword(weapon,direction)){
        q.windup=chain?(direction==4?.23:.24):direction==0?.32:direction==1?.30:.28;
        q.strike=direction==4?.16:.18;q.recovery=.30;q.link=.14;
    }else if(weapon==world_weapon_loadout::Weapon::Sword&&direction==2&&std::isfinite(sword_high_min)){
        q.windup=std::max(q.windup,sword_high_min);
    }else if(weapon==world_weapon_loadout::Weapon::SwordShield){
        // One-handed cuts remain nimble; their actual release is no longer a
        // universal 90ms snap shared with a massive axe.
        q.windup=chain?(direction==4?.20:.21):(direction==2?.31:direction==4?.25:.28);
        q.strike=direction==4?.14:.16;q.recovery=.26;q.link=.12;
    }else if(weapon==world_weapon_loadout::Weapon::GreatAxe){
        q.windup=chain?(direction==4?.27:.30):(direction==2?.44:direction==4?.34:.40);
        q.strike=direction==4?.20:.24;q.recovery=.40;q.link=.18;
    }else if(weapon==world_weapon_loadout::Weapon::Staff){
        q.windup=chain?.18:(direction==2?.28:direction==4?.22:.25);
        q.strike=direction==4?.14:.16;q.recovery=.24;q.link=.10;
    }
    // Keep the authored acceleration/contact/recovery proportions together.
    // Both the displayed pose and swept blade consume this committed clock.
    q.windup*=.90;q.strike*=.90;q.recovery*=.90;q.link*=.90;
    return q;
}
}
#endif

#ifndef VALWOOD_WORLD_ATTACK_FOOT_CONTACT_H
#define VALWOOD_WORLD_ATTACK_FOOT_CONTACT_H
#include <algorithm>
#include <cmath>
// Captured attack soles supply contact intent. Only the existing unchanged-
// length leg IK may realize that intent; root, pelvis and weapon stay authored.
namespace world_attack_foot_contact {
inline float smooth(float value){value=std::clamp(value,0.f,1.f);return value*value*(3-2*value);}
inline float confidence(double authored_boot_height,double horizontal_speed,double vertical_speed){
 if(!std::isfinite(authored_boot_height)||!std::isfinite(horizontal_speed)||!std::isfinite(vertical_speed))return 0;
 // A quiet sole below2.5cm may support; a lifted6.5cm boot or a fast
 // captured pivot releases. Ground clearance already handles downward
 // capture overshoot, so low soles are not mistaken for an airborne step.
 return smooth(float((.065-authored_boot_height)/.040))*smooth(float((1.50-horizontal_speed)/.85))*(authored_boot_height<=.025?1.f:smooth(float((.80-std::abs(vertical_speed))/.50)));
}
inline double advance(double phase,double seconds,double windup,double strike,double recovery){
 const double t=phase<.36?phase/.36*windup:phase<.60?windup+(phase-.36)/.24*strike:windup+strike+(phase-.60)/.40*recovery;
 const double next=std::clamp(t+seconds,0.,windup+strike+recovery);
 return next<windup?.36*next/windup:next<windup+strike?.36+.24*(next-windup)/strike:.60+.40*(next-windup-strike)/recovery;
}
inline float blend(float locomotion,float captured,float locomotion_legs){
 const float weight=std::clamp(locomotion_legs,0.f,1.f);
 return std::clamp(captured+(locomotion-captured)*weight,0.f,1.f);
}
}
#endif

#ifndef VALWOOD_WORLD_EQUIPMENT_PRESENTATION_H
#define VALWOOD_WORLD_EQUIPMENT_PRESENTATION_H
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

// Presentation clock only: gameplay remains responsible for accepting a
// stow/draw request at a safe action boundary. No combat phase is advanced here.
namespace world_equipment_presentation {
enum class Phase {Armed,Lowering,Stowed,Raising};
class State {
public:
 static constexpr double transition_seconds=.40;
 void reset(bool stowed=false){requested_=stowed;phase_=stowed?Phase::Stowed:Phase::Armed;elapsed_=0;visible_=!stowed;serial_=0;}
 // A changed request publishes the exact entry frame before elapsed advances.
 // Reversal begins at the displayed pose, with the existing visibility latch.
 bool advance(bool stowed,double dt,bool paused=false){
  if(paused||!std::isfinite(dt)||dt<0)return false;
  if(stowed!=requested_){requested_=stowed;phase_=stowed?Phase::Lowering:Phase::Raising;elapsed_=0;++serial_;return true;}
  if(!transitioning())return false;
  elapsed_=std::min(transition_seconds,elapsed_+std::min(dt,.1));
  if(phase_==Phase::Lowering&&progress()>=.72)visible_=false;
  if(phase_==Phase::Raising&&progress()>=.25)visible_=true;
  if(elapsed_>=transition_seconds){phase_=requested_?Phase::Stowed:Phase::Armed;visible_=!requested_;}
  return false;
 }
 Phase phase()const{return phase_;}
 double progress()const{return std::clamp(elapsed_/transition_seconds,0.,1.);}
 bool transitioning()const{return phase_==Phase::Lowering||phase_==Phase::Raising;}
 bool weapon_visible()const{return visible_;}
 bool combat_ready()const{return phase_==Phase::Armed;}
 bool requested_stowed()const{return requested_;}
 std::uint64_t serial()const{return serial_;}
private:
 Phase phase_=Phase::Armed;double elapsed_=0;bool requested_=false,visible_=true;std::uint64_t serial_=0;
};

// Host-side input latch: drawing never turns a held attack/guard into a fresh
// action. It rearms only after all action controls have been released in Armed.
class ActionGate {
public:
 void reset(){armed_=false;}
 bool update(bool ready,bool attack_down,bool guard_down){
  if(!ready){armed_=false;return false;}
  if(!attack_down&&!guard_down)armed_=true;
  return armed_;
 }
 bool armed()const{return armed_;}
private:bool armed_=false;
};

// One bounded host transaction. Inventory/weapon identity commits only after
// old gear is lowered. A new pose is seeded hidden before raising begins.
enum class ChangeAction {None,Commit,BeginDraw,Completed};
// A handled request can be rejected, a no-op, a draw or a queued replacement.
// Keep this distinct from the callback's ownership/handled boolean.
enum class RequestResult {None,Rejected,AlreadyEquipped,DrawRequested,Queued};
struct RequestFeedback {
 RequestResult result=RequestResult::None;
 std::string message;
 void set(RequestResult next,const char *text){result=next;message=text;}
 void clear(){result=RequestResult::None;message.clear();}
};
class Change {
public:
 void request(unsigned target){target_=target;phase_=1;seconds_=0;pose_seen_=false;}
 void cancel(){phase_=0;seconds_=0;pose_seen_=false;}
 bool active()const{return phase_!=0;}
 unsigned target()const{return target_;}
 bool lowering()const{return phase_==1;}
 void committed(bool success){if(success){phase_=3;pose_seen_=false;}else cancel();}
 void pose_committed(){if(phase_==3)pose_seen_=true;}
 ChangeAction advance(double dt,bool paused,bool bow,bool lowered,bool ready){
  if(paused||!std::isfinite(dt)||dt<0)return ChangeAction::None;
  if(phase_==1){seconds_+=std::min(dt,.1);if((bow&&seconds_>=.24)||(!bow&&lowered)){phase_=2;return ChangeAction::Commit;}}
  else if(phase_==3&&pose_seen_){phase_=4;seconds_=0;return ChangeAction::BeginDraw;}
  else if(phase_==4){seconds_+=std::min(dt,.1);if((bow&&seconds_>=.24)||(!bow&&ready)){cancel();return ChangeAction::Completed;}}
  return ChangeAction::None;
 }
private:unsigned target_=0,phase_=0;double seconds_=0;bool pose_seen_=false;
};
}
#endif

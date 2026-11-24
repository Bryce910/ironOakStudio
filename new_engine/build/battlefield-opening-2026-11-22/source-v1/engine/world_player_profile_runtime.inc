world_player_profile::State player_profile;
std::filesystem::path player_profile_path="saves/player.progress";
bool player_profile_ready=true,player_profile_exists=false;
std::string player_profile_error;
if(arena_enabled||first_journey_enabled){
 if(std::ifstream cfg{"player-profile.config"};cfg){
  std::string tag,path,extra;unsigned v=0;
  if(!(cfg>>tag>>v)||tag!="VALWOOD_PLAYER_PROFILE"||v!=1||!(cfg>>tag>>path)||tag!="path"||path.size()>512||!(cfg>>tag)||tag!="end"||(cfg>>extra)){
   std::fprintf(stderr,"[Player] Invalid player-profile.config\n");return 1;
  }
  auto relative=std::filesystem::path(path);
  if(relative.is_absolute()||relative.filename()!="player.progress"){std::fprintf(stderr,"[Player] Invalid profile path\n");return 1;}
  player_profile_path=relative;
 }
 const auto loaded=world_player_profile::load(player_profile_path,player_profile,player_profile_error);
 player_profile_ready=loaded==world_arena_progress::Load::Missing||loaded==world_arena_progress::Load::Loaded;
 player_profile_exists=loaded==world_arena_progress::Load::Loaded;
 if(!player_profile_ready){std::fprintf(stderr,"[Player] %s\n",player_profile_error.c_str());return 1;}
}
auto player_profile_commit=[&](world_player_profile::State next,std::string &error){
 if(!player_profile_ready){error=player_profile_error;return false;}
 // Protect against a stale second process or an external save replacement.
 world_player_profile::State disk;const auto loaded=world_player_profile::load(player_profile_path,disk,error);
 if((player_profile_exists&&(loaded!=world_arena_progress::Load::Loaded||world_player_profile::encode(disk)!=world_player_profile::encode(player_profile)))||
    (!player_profile_exists&&loaded!=world_arena_progress::Load::Missing)){
  error="Player progress changed in another session. Close and reopen this session before saving.";return false;
 }
 next.revision=player_profile.revision+1;
 if(!world_player_profile::save(player_profile_path,next,error))return false;
 player_profile=next;player_profile_exists=true;return true;
};
auto player_profile_balance=[&](){return world_player_inventory::purse(player_profile.inventory,world_player_profile::earned(player_profile));};

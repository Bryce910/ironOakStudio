#ifndef VALWOOD_WORLD_ARENA_OPPONENTS_H
#define VALWOOD_WORLD_ARENA_OPPONENTS_H

// The committed run owns species. A setup draft never changes live actors.
namespace world_arena_opponents {
enum class Kind : unsigned {Human=0,Orc=1};
inline constexpr bool valid(Kind kind){return kind==Kind::Human||kind==Kind::Orc;}
inline constexpr const char *name(Kind kind){switch(kind){
    case Kind::Human:return "Humans";
    case Kind::Orc:return "Orcs";
}return "Unavailable opponents";}
inline constexpr bool available(Kind kind,bool orc_assets_ready){return valid(kind)&&(kind==Kind::Human||orc_assets_ready);}
}
#endif

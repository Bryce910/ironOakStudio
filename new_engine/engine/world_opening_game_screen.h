#ifndef VALWOOD_WORLD_OPENING_GAME_SCREEN_H
#define VALWOOD_WORLD_OPENING_GAME_SCREEN_H
#include <array>
// Only the opted-in apartment host drives these values. A default zero keeps
// the existing TV animation and every other project's materials unchanged.
namespace world_opening_game_screen {
inline std::array<float,4> state={{0,0,0,0}};
}
#endif

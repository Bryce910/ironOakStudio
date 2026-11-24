#ifndef VALWOOD_WORLD_VILLAGE_GROUND_H
#define VALWOOD_WORLD_VILLAGE_GROUND_H
// Linear UNORM material-history weights, never albedo or baked illumination.
// PNG row0 maps to minimum local Z; upload without an image Y flip.
inline constexpr int world_village_ground_side=2049;
inline constexpr double world_village_ground_min_x=-225.,world_village_ground_min_z=-225.;
inline constexpr double world_village_ground_width=450.,world_village_ground_depth=450.;
inline constexpr const char *world_village_ground_file="ground/village_ground_history.png";
// R soil deposited against actual ground sections; G traffic burnish;
// B localized damp drainage; A sediment at the authored paving boundary.
#endif

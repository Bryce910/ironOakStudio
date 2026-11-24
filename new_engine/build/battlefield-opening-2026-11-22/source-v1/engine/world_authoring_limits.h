#ifndef VALWOOD_WORLD_AUTHORING_LIMITS_H
#define VALWOOD_WORLD_AUTHORING_LIMITS_H
// Source-scene overrides and a complete imported construction palette coexist.
inline constexpr unsigned WORLD_CUSTOM_ASSET_LIMIT=1024;
// Source records and editor placement keys must share a non-overlapping range.
inline constexpr unsigned WORLD_LEGACY_SOURCE_RECORD_LIMIT=16384;
inline constexpr unsigned WORLD_SOURCE_RECORD_LIMIT=65536;
inline constexpr unsigned WORLD_ADDED_KEY_BASE=WORLD_SOURCE_RECORD_LIMIT;
// Total scene capacity is independent of the unchanged GPU per-alias buffer.
inline constexpr unsigned WORLD_SCENERY_ALIAS_RECORD_LIMIT=4096;
inline constexpr unsigned WORLD_SCENERY_SOURCE_FILE_LIMIT=16;
inline constexpr float WORLD_OBJECT_OFFSET_LIMIT=128.f;
#endif

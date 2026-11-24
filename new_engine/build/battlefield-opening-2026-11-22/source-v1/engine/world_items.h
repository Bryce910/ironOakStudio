#ifndef ENGINE_WORLD_ITEMS_H
#define ENGINE_WORLD_ITEMS_H

#include "engine/runtime_types.h"

#define AUTO_PICKUP 0
#define MAX_WORLD_ITEMS 64
#define PICKUP_RADIUS 1.8f

typedef struct WorldItem{
    ItemType type;
    int count;
    Vec3 pos;
    bool active;
    float bob;
    uint8_t rarity;
    uint8_t affix;
}WorldItem;

typedef struct WorldItemList{
    WorldItem items[MAX_WORLD_ITEMS];
    int count;
}WorldItemList;

#endif

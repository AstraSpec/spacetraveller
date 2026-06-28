#ifndef SPACETRAVELLER_CAVE_GENERATOR_H
#define SPACETRAVELLER_CAVE_GENERATOR_H

#include "world/dungeon_generator.h"

namespace godot {

struct DungeonInfo;

class CaveGenerator {
public:
    static DungeonLayout build_layout(const DungeonInfo& p_info, const Vector2i& p_entrance_chunk, int p_world_seed);
};

}

#endif // SPACETRAVELLER_CAVE_GENERATOR_H

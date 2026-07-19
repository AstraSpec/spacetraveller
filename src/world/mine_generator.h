#ifndef SPACETRAVELLER_MINE_GENERATOR_H
#define SPACETRAVELLER_MINE_GENERATOR_H

#include "dungeon_generator.h"

namespace godot {

struct DungeonInfo;

class MineGenerator {
public:
    static DungeonLayout build_layout(
        const DungeonInfo& p_info,
        const Vector2i& p_entrance_chunk,
        int p_world_seed
    );
};

}

#endif // SPACETRAVELLER_MINE_GENERATOR_H

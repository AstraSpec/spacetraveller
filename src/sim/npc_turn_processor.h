#ifndef SPACETRAVELLER_NPC_TURN_PROCESSOR_H
#define SPACETRAVELLER_NPC_TURN_PROCESSOR_H

#include <cstdint>

namespace godot {

class EntityPool;
class TileDb;
class SimulationDirector;

class NpcTurnProcessor {
public:
    static void run_turn(
        uint32_t entity_id,
        EntityPool& pool,
        TileDb& tile_db,
        SimulationDirector& director
    );
};

}

#endif

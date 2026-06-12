#ifndef SPACETRAVELLER_NPC_TURN_PROCESSOR_H
#define SPACETRAVELLER_NPC_TURN_PROCESSOR_H

#include <cstdint>
#include <vector>
#include <godot_cpp/variant/vector2i.hpp>

namespace godot {

class EntityPool;
class Entity;
class TileDb;
class SimulationDirector;
struct Intent;
struct LocomotionData;
struct AIData;

class NpcTurnProcessor {
public:
    static void run_turn(
        uint32_t entity_id,
        EntityPool& pool,
        TileDb& tile_db,
        SimulationDirector& director
    );

private:
    static float resolve_move(
        uint32_t entity_id,
        Entity& entity,
        const Intent& intent,
        LocomotionData& loco,
        AIData& ai,
        std::vector<Vector2i>& blocking_positions,
        SimulationDirector& director
    );
};

}

#endif

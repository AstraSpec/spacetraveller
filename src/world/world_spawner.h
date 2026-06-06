#ifndef SPACETRAVELLER_WORLD_SPAWNER_H
#define SPACETRAVELLER_WORLD_SPAWNER_H

#include <godot_cpp/variant/vector2i.hpp>
#include <cstdint>
#include <vector>

namespace godot {

class EntityLedger;
class TurnScheduler;
class WorldBubble;
class WorldGenerator;
class WorldSpawnState;

class WorldSpawner {
public:
    static void spawn_for_newly_seen_cells(
        uint32_t p_world_seed,
        float p_spawn_turn_time,
        const std::vector<uint64_t>& p_newly_seen_cells,
        WorldGenerator& p_generator,
        WorldBubble& p_bubble,
        EntityLedger& p_ledger,
        TurnScheduler& p_scheduler,
        WorldSpawnState& p_spawn_state
    );
};

}

#endif // SPACETRAVELLER_WORLD_SPAWNER_H

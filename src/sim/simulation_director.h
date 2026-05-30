#ifndef SPACETRAVELLER_SIMULATION_DIRECTOR_H
#define SPACETRAVELLER_SIMULATION_DIRECTOR_H

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <cstdint>

#include "sim/simulation_event_sink.h"
#include "entities/entity.h"
#include "entities/entity_ledger.h"
#include "world/world_bubble.h"
#include "world/turn_scheduler.h"
#include "path/a_star_grid.h"

namespace godot {

struct SimulationDirectorDeps {
    EntityLedger* ledger = nullptr;
    WorldBubble* bubble = nullptr;
    AStarGridPathfinder* pathfinder = nullptr;
    TurnScheduler* scheduler = nullptr;
    ISimulationEventSink* sink = nullptr;
    uint32_t player_entity_id = PLAYER_ENTITY_ID;
};

class SimulationDirector {
public:
    SimulationDirector() = default;

    void configure(const SimulationDirectorDeps& deps);

    float submit_player_intent(int intent_type, int target_x, int target_y, const String& param);
    void process_game_turn(float current_time);

    Array find_path(const Vector2i& start, const Vector2i& goal);
    Array request_player_path(const Vector2i& start, const Vector2i& goal);

private:
    Array find_path_with_flags(const Vector2i& start, const Vector2i& goal, uint32_t flags);
    void despawn_entity(uint32_t entity_id);
    Vector2i entity_chunk(uint32_t entity_id) const;
    void apply_attack_effects(uint32_t attacker_id, uint32_t defender_id, const struct AttackResult& atk);
    void advance_entity_time(uint32_t entity_id, float dt);

    SimulationDirectorDeps d;
};

}

#endif // SPACETRAVELLER_SIMULATION_DIRECTOR_H

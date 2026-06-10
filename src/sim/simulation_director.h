#ifndef SPACETRAVELLER_SIMULATION_DIRECTOR_H
#define SPACETRAVELLER_SIMULATION_DIRECTOR_H

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <cstdint>

#include "sim/simulation_event_sink.h"
#include "sim/game_event.h"
#include "entities/entity.h"
#include "entities/entity_ledger.h"
#include "world/world_bubble.h"
#include "world/turn_scheduler.h"
#include "path/a_star_grid.h"
#include "core/rng.h"

namespace godot {

struct CombatOutcome;

struct SimulationDirectorDeps {
    EntityLedger* ledger = nullptr;
    WorldBubble* bubble = nullptr;
    AStarGridPathfinder* pathfinder = nullptr;
    TurnScheduler* scheduler = nullptr;
    ISimulationEventSink* sink = nullptr;
    IGameEventListener* event_listener = nullptr;
    uint32_t player_entity_id = PLAYER_ENTITY_ID;
    const int* world_seed = nullptr;
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
    Vector2i entity_chunk(uint32_t entity_id) const;
    CombatOutcome resolve_entity_attack(uint32_t attacker_id, uint32_t defender_id);
    void handle_entity_death(uint32_t entity_id, const String& cause, uint32_t killer_id);
    bool finish_entity_action(uint32_t entity_id, float cost, float base_time);
    void emit_movement_if_needed(uint32_t entity_id, const Vector2i& old_pos);
    void apply_attack_effects(uint32_t attacker_id, uint32_t defender_id, const CombatOutcome& atk);
    void advance_entity_time(uint32_t entity_id, float dt);
    float entity_base_damage(uint32_t entity_id) const;
    String entity_faction(uint32_t entity_id) const;
    uint32_t find_nearest_hostile(uint32_t entity_id, int radius) const;
    Rng::Seeded combat_rng_for(uint32_t attacker_id, uint32_t defender_id) const;

    SimulationDirectorDeps d;
};

}

#endif // SPACETRAVELLER_SIMULATION_DIRECTOR_H

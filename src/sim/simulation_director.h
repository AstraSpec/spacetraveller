#ifndef SPACETRAVELLER_SIMULATION_DIRECTOR_H
#define SPACETRAVELLER_SIMULATION_DIRECTOR_H

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <cstdint>
#include <vector>

#include "sim/simulation_event_sink.h"
#include "sim/game_event.h"
#include "entities/entity.h"
#include "entities/entity_ledger.h"
#include "world/world_bubble.h"
#include "world/turn_scheduler.h"
#include "path/a_star_grid.h"
#include "core/rng.h"
#include "components/ai_controller.h"

namespace godot {

struct CombatOutcome;
struct LocomotionData;
struct Intent;
struct ActionResult;
enum class ActionFailure;
struct RaceInfo;
class TileDb;
class EntityTracker;
class PointOfInterestRegistry;
class CityPopulationDirector;

struct SimulationDirectorDeps {
    EntityLedger* ledger = nullptr;
    EntityTracker* tracker = nullptr;
    WorldBubble* bubble = nullptr;
    AStarGridPathfinder* pathfinder = nullptr;
    TurnScheduler* scheduler = nullptr;
    ISimulationEventSink* sink = nullptr;
    IGameEventListener* event_listener = nullptr;
    PointOfInterestRegistry* poi_registry = nullptr;
    CityPopulationDirector* city_population = nullptr;
    uint32_t player_entity_id = PLAYER_ENTITY_ID;
    const int* world_seed = nullptr;
};

class SimulationDirector {
    friend class NpcTurnProcessor;
public:
    SimulationDirector() = default;

    void configure(const SimulationDirectorDeps& deps);

    float submit_player_intent(int intent_type, int target_x, int target_y, const String& param);
    float submit_player_targeted_attack(uint32_t target_id, const String& ability_id, int body_part_index);
    float submit_player_change_z(int delta);
    bool submit_pickup(uint32_t entity_id, const Vector2i& pos, const String& item_id, int amount);
    void process_game_turn(float current_time);
    bool start_player_crafting(const String& recipe_id);
    void process_player_activity_batch();
    bool request_activity_interruption(const String& interruption_id, uint32_t source_entity = UINT32_MAX);
    bool resolve_player_activity_interruption(const String& resolution);
    Dictionary get_player_activity() const;
    bool has_player_activity() const;
    void restore_player_activity();

    bool submit_player_drop(const String& item_id, int amount);
    bool submit_player_wield(const String& item_id);
    bool submit_player_unwield(const String& slot_name);
    bool submit_player_wear(const String& item_id);
    bool submit_player_remove_clothing(const String& item_id);
    String get_player_movement_mode() const;
    bool set_player_movement_mode(const String& mode_id, const String& reason = "selected");
    bool toggle_player_run();
    Array get_player_movement_mode_options() const;
    float get_entity_effective_movement_speed(uint32_t entity_id) const;

    Array find_path(const Vector2i& start, const Vector2i& goal);
    Array request_player_path(const Vector2i& start, const Vector2i& goal);
    bool entity_is_hostile_to(uint32_t entity_id, uint32_t target_id) const;
    bool can_interact_with_entity(uint32_t entity_id) const;
    bool set_entity_relation(uint32_t entity_id, uint32_t target_id, const String& relation);
    String get_entity_relation(uint32_t entity_id, uint32_t target_id) const;
    bool start_entity_follow(uint32_t entity_id);
    bool set_entity_behavior(uint32_t entity_id, const String& state, uint32_t target_id);
    String get_entity_behavior_state(uint32_t entity_id) const;
    uint32_t get_entity_behavior_target(uint32_t entity_id) const;
    Array get_player_attack_options(uint32_t target_id);
    Array get_entity_targetable_body_parts(uint32_t entity_id) const;

private:
    Array find_path_with_flags(const Vector2i& start, const Vector2i& goal, uint32_t flags);
    Vector2i entity_chunk(uint32_t entity_id) const;
    CombatOutcome resolve_entity_attack(
        uint32_t attacker_id,
        uint32_t defender_id,
        const String& ability_id = String(),
        int body_part_index = -1
    );
    bool plan_player_intent(Intent& intent);
    ActionResult resolve_player_action(const Intent& intent, Entity& entity, LocomotionData& loco);
    ActionResult resolve_player_attack(const Intent& intent);
    ActionResult resolve_player_smash(const Intent& intent, Entity& entity, LocomotionData& loco);
    ActionResult resolve_player_basic_action(const Intent& intent, Entity& entity, LocomotionData& loco);
    ActionResult resolve_player_pickup(const Intent& intent);
    ActionResult resolve_pickup(uint32_t entity_id, const Intent& intent);
    bool finish_player_action(const ActionResult& result, float base_time, const Vector2i& old_pos, int old_z);
    Rng::Seeded action_rng_for(uint32_t entity_id, const Vector2i& target, Rng::Stream stream) const;
    float resolve_attack(
        uint32_t attacker_id,
        uint32_t defender_id,
        bool is_player,
        const String& ability_id = String(),
        int body_part_index = -1
    );
    void notify_attack(uint32_t attacker_id, uint32_t defender_id);
    float execute_player_intent(Intent intent);
    void handle_entity_death(uint32_t entity_id, const String& cause, uint32_t killer_id);
    bool finish_entity_action(
        uint32_t entity_id,
        float cost,
        float base_time,
        float stamina_cost = 0.0f,
        bool suppress_stamina_recovery = false
    );
    void emit_movement_if_needed(uint32_t entity_id, const Vector2i& old_pos);
    float movement_action_cost(uint32_t entity_id, float base_cost, const LocomotionData& loco) const;
    float entity_moving_capacity(uint32_t entity_id) const;
    void emit_player_action_failure(ActionFailure failure) const;
    bool can_player_run() const;
    void force_player_walk_if_exhausted();
    void apply_attack_effects(uint32_t attacker_id, uint32_t defender_id, const CombatOutcome& atk);
    void advance_entity_time(uint32_t entity_id, float dt);
    float entity_base_damage(uint32_t entity_id) const;
    String entity_faction(uint32_t entity_id) const;
    Rng::Seeded combat_rng_for(uint32_t attacker_id, uint32_t defender_id) const;
    const RaceInfo* get_race_info(uint32_t entity_id) const;
    static uint64_t entity_rng_salt(const Entity* entity, uint32_t entity_id);
    void schedule_next_activity_work(ActivityData& activity);
    void emit_activity_checkpoint(ActivityData& activity);
    void emit_activity_interrupted(ActivityData& activity);
    void cancel_player_activity(const String& reason, bool restore_menu = true);
    void complete_player_activity();
    bool validate_crafting_requirements(const String& recipe_id) const;
    bool complete_crafting(const String& recipe_id, Dictionary& completion);
    bool finish_short_player_action();
    bool processing_activity_batch = false;

    SimulationDirectorDeps d;
};

}

#endif // SPACETRAVELLER_SIMULATION_DIRECTOR_H

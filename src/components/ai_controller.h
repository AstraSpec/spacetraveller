#ifndef SPACETRAVELLER_AI_CONTROLLER_H
#define SPACETRAVELLER_AI_CONTROLLER_H

#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <cstdint>
#include <functional>
#include <limits>
#include "action_resolver.h"

namespace godot {

struct LocomotionData;
struct PathResult;
struct Entity;

enum class AIState { WANDER, COMBAT, IDLE, FOLLOW, FLEE, GUARD };
enum class ReactionPolicy { PASSIVE, TIMID, DEFENSIVE, AGGRESSIVE, PREDATORY };
enum class RoutinePhase { SEEKING, TRAVELLING, DWELLING };

struct AIData {
    AIState state = AIState::WANDER;
    AIState home_state = AIState::WANDER;
    ReactionPolicy reaction_policy = ReactionPolicy::DEFENSIVE;
    int reaction_radius = 12;
    Vector2i home_position;
    uint32_t target_entity_id = std::numeric_limits<uint32_t>::max();
    // The leader remains separate from target_entity_id so an escort can
    // temporarily enter combat and then resume following afterward.
    uint32_t follow_leader_id = std::numeric_limits<uint32_t>::max();
    bool has_last_known_target_position = false;
    Vector2i last_known_target_position;
    int lost_target_turns = 0;
    float wander_radius = 4.0f;
    int calm_scan_countdown = 0;
    int wait_turns = 0;
    int blocked_move_count = 0;
    int path_retry_countdown = 0;
    bool forced_reaction = false;
    bool has_routine_scope = false;
    String routine_structure_id;
    Vector3i routine_scope_origin;
    RoutinePhase routine_phase = RoutinePhase::SEEKING;
    bool routine_has_target = false;
    Vector3i routine_target;
    bool routine_has_last_position = false;
    Vector3i routine_last_position;
    int routine_dwell_remaining = 0;
    int routine_retry_turns = 0;
    int routine_failed_attempts = 0;
};

struct AIContext {
    const Entity& self;
    const Vector2i& target_pos;
    uint32_t target_entity_id = std::numeric_limits<uint32_t>::max();
    bool has_target = false;
    bool target_visible = false;
    bool target_same_level = true;
    bool has_routine_goal = false;
    Vector2i routine_goal;
    const std::function<bool(Vector2i)>& can_enter_terrain;
    const std::function<PathResult(Vector2i, Vector2i, int)>& find_path;
};

namespace AIController {
    Intent tick(AIData& ai, LocomotionData& loco, const AIContext& ctx);
    void reset_routine(AIData& ai, bool clear_history, int retry_turns = 0);
    AIState state_from_string(const String& value);
    String state_to_string(AIState value);
    bool is_valid_state_name(const String& value);
    ReactionPolicy reaction_policy_from_string(const String& value);
    String reaction_policy_to_string(ReactionPolicy value);
    Dictionary serialize(const AIData& data);
    void deserialize(AIData& data, const Dictionary& dict);
}

}

#endif // SPACETRAVELLER_AI_CONTROLLER_H

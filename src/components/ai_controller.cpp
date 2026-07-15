#include "ai_controller.h"

#include "locomotion.h"
#include "entities/entity.h"
#include "path/path_result.h"

#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

namespace {

constexpr int FOLLOW_RADIUS = 2;

int chebyshev_distance(const Vector2i& a, const Vector2i& b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

bool is_adjacent(const Vector2i& a, const Vector2i& b) {
    return a != b && chebyshev_distance(a, b) <= 1;
}

Intent move_toward(
    AIData& ai,
    LocomotionData& loco,
    const AIContext& ctx,
    const Vector2i& goal,
    int goal_radius
) {
    const Vector2i self_pos(ctx.self.x, ctx.self.y);
    goal_radius = std::max(0, goal_radius);
    if (chebyshev_distance(self_pos, goal) <= goal_radius) {
        Locomotion::clear_path(loco);
        ai.blocked_move_count = 0;
        return Intent{};
    }

    Vector2i next;
    if (Locomotion::peek_next_step(loco, next)) {
        const bool endpoint_valid = chebyshev_distance(loco.path_goal, goal) <= goal_radius;
        if (endpoint_valid && is_adjacent(self_pos, next) && ctx.can_enter_terrain(next)) {
            return Intent{IntentType::MOVE, next};
        }
        Locomotion::clear_path(loco);
    }

    if (ai.path_retry_countdown > 0) {
        --ai.path_retry_countdown;
        return Intent{};
    }

    PathResult result = ctx.find_path(self_pos, goal, goal_radius);
    if (!result.found || result.waypoints.empty()) {
        ai.path_retry_countdown = UtilityFunctions::randi_range(2, 4);
        return Intent{};
    }

    const Vector2i endpoint = result.waypoints.back();
    Locomotion::set_path(loco, result.waypoints, endpoint, goal_radius);
    ai.path_retry_countdown = 0;
    if (Locomotion::peek_next_step(loco, next)) {
        return Intent{IntentType::MOVE, next};
    }
    return Intent{};
}

Vector2i pick_wander_target(const AIData& ai, const Entity& self, const AIContext& ctx) {
    const int radius = std::max(1, static_cast<int>(ai.wander_radius));
    for (int attempt = 0; attempt < 8; ++attempt) {
        const Vector2i candidate(
            ai.home_position.x + UtilityFunctions::randi_range(-radius, radius),
            ai.home_position.y + UtilityFunctions::randi_range(-radius, radius)
        );
        if (candidate != Vector2i(self.x, self.y) && ctx.can_enter_terrain(candidate)) {
            return candidate;
        }
    }
    return Vector2i(self.x, self.y);
}

Intent flee_locally(AIData& ai, LocomotionData& loco, const AIContext& ctx) {
    static const Vector2i directions[] = {
        Vector2i(0, -1), Vector2i(1, -1), Vector2i(1, 0), Vector2i(1, 1),
        Vector2i(0, 1), Vector2i(-1, 1), Vector2i(-1, 0), Vector2i(-1, -1)
    };

    Locomotion::clear_path(loco);
    const Vector2i self_pos(ctx.self.x, ctx.self.y);
    const int current_distance = chebyshev_distance(self_pos, ctx.target_pos);
    Vector2i best = self_pos;
    int best_distance = current_distance;
    const int direction_offset = static_cast<int>(ctx.self.id % 8);
    for (int i = 0; i < 8; ++i) {
        const Vector2i candidate = self_pos + directions[(i + direction_offset) % 8];
        if (!ctx.can_enter_terrain(candidate)) continue;
        const int distance = chebyshev_distance(candidate, ctx.target_pos);
        if (distance > best_distance) {
            best = candidate;
            best_distance = distance;
        }
    }
    return best == self_pos ? Intent{} : Intent{IntentType::MOVE, best};
}

}

AIState AIController::state_from_string(const String& value) {
    const String normalized = value.to_lower();
    if (normalized == "wander") return AIState::WANDER;
    if (normalized == "combat") return AIState::COMBAT;
    if (normalized == "idle") return AIState::IDLE;
    if (normalized == "follow") return AIState::FOLLOW;
    if (normalized == "flee") return AIState::FLEE;
    if (normalized == "guard") return AIState::GUARD;
    return AIState::WANDER;
}

String AIController::state_to_string(AIState value) {
    switch (value) {
        case AIState::WANDER: return "wander";
        case AIState::COMBAT: return "combat";
        case AIState::IDLE: return "idle";
        case AIState::FOLLOW: return "follow";
        case AIState::FLEE: return "flee";
        case AIState::GUARD: return "guard";
    }
    return "wander";
}

bool AIController::is_valid_state_name(const String& value) {
    const String normalized = value.to_lower();
    return normalized == "wander" || normalized == "combat" || normalized == "idle" ||
        normalized == "follow" || normalized == "flee" || normalized == "guard";
}

ReactionPolicy AIController::reaction_policy_from_string(const String& value) {
    const String normalized = value.to_lower();
    if (normalized == "passive") return ReactionPolicy::PASSIVE;
    if (normalized == "timid") return ReactionPolicy::TIMID;
    if (normalized == "aggressive") return ReactionPolicy::AGGRESSIVE;
    if (normalized == "predatory") return ReactionPolicy::PREDATORY;
    return ReactionPolicy::DEFENSIVE;
}

String AIController::reaction_policy_to_string(ReactionPolicy value) {
    switch (value) {
        case ReactionPolicy::PASSIVE: return "passive";
        case ReactionPolicy::TIMID: return "timid";
        case ReactionPolicy::AGGRESSIVE: return "aggressive";
        case ReactionPolicy::PREDATORY: return "predatory";
        case ReactionPolicy::DEFENSIVE: break;
    }
    return "defensive";
}

Intent AIController::tick(AIData& ai, LocomotionData& loco, const AIContext& ctx) {
    const Vector2i self_pos(ctx.self.x, ctx.self.y);

    if (ai.state == AIState::COMBAT) {
        if (!ctx.has_target || !ctx.target_same_level) return Intent{};
        if (ctx.target_visible && is_adjacent(self_pos, ctx.target_pos)) {
            Intent attack;
            attack.type = IntentType::ATTACK;
            attack.target = ctx.target_pos;
            attack.entity_target_id = ctx.target_entity_id;
            ai.blocked_move_count = 0;
            return attack;
        }
        if (!ctx.target_visible && self_pos == ctx.target_pos) {
            Locomotion::clear_path(loco);
            return Intent{};
        }
        return move_toward(ai, loco, ctx, ctx.target_pos, ctx.target_visible ? 1 : 0);
    }

    if (ai.state == AIState::FLEE) {
        if (!ctx.has_target) return Intent{};
        return flee_locally(ai, loco, ctx);
    }

    if (ai.state == AIState::FOLLOW) {
        if (!ctx.has_target || !ctx.target_same_level) {
            Locomotion::clear_path(loco);
            return Intent{};
        }
        return move_toward(ai, loco, ctx, ctx.target_pos, FOLLOW_RADIUS);
    }

    if (ai.state == AIState::IDLE || ai.state == AIState::GUARD) {
        return move_toward(ai, loco, ctx, ai.home_position, 0);
    }

    if (ai.wait_turns > 0) {
        --ai.wait_turns;
        return Intent{};
    }

    Vector2i wander_goal;
    if (Locomotion::has_path(loco)) {
        wander_goal = loco.path_goal;
    } else {
        if (ai.path_retry_countdown > 0) {
            --ai.path_retry_countdown;
            return Intent{};
        }
        wander_goal = pick_wander_target(ai, ctx.self, ctx);
        if (wander_goal == self_pos) {
            ai.wait_turns = UtilityFunctions::randi_range(2, 4);
            return Intent{};
        }
    }
    return move_toward(ai, loco, ctx, wander_goal, 0);
}

Dictionary AIController::serialize(const AIData& data) {
    Dictionary d;
    d["state"] = state_to_string(data.state);
    d["home_state"] = state_to_string(data.home_state);
    d["reaction_policy"] = reaction_policy_to_string(data.reaction_policy);
    d["reaction_radius"] = data.reaction_radius;
    d["home_x"] = data.home_position.x;
    d["home_y"] = data.home_position.y;
    if (data.target_entity_id != std::numeric_limits<uint32_t>::max()) {
        d["target_entity_id"] = static_cast<int64_t>(data.target_entity_id);
    }
    if (data.follow_leader_id != std::numeric_limits<uint32_t>::max()) {
        d["follow_leader_id"] = static_cast<int64_t>(data.follow_leader_id);
    }
    if (data.has_last_known_target_position) {
        d["last_known_target_x"] = data.last_known_target_position.x;
        d["last_known_target_y"] = data.last_known_target_position.y;
    }
    d["lost_target_turns"] = data.lost_target_turns;
    d["wander_radius"] = data.wander_radius;
    d["wait_turns"] = data.wait_turns;
    d["forced_reaction"] = data.forced_reaction;
    return d;
}

void AIController::deserialize(AIData& data, const Dictionary& dict) {
    data.state = state_from_string(String(dict.get("state", "wander")));
    data.home_state = state_from_string(String(dict.get("home_state", "wander")));
    data.reaction_policy = reaction_policy_from_string(String(dict.get("reaction_policy", "defensive")));
    data.reaction_radius = std::max(1, static_cast<int>(dict.get("reaction_radius", 12)));
    data.home_position = Vector2i(
        static_cast<int>(dict.get("home_x", 0)),
        static_cast<int>(dict.get("home_y", 0))
    );
    data.target_entity_id = std::numeric_limits<uint32_t>::max();
    const int64_t target_id = static_cast<int64_t>(dict.get("target_entity_id", static_cast<int64_t>(-1)));
    if (target_id >= 0 && target_id <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
        data.target_entity_id = static_cast<uint32_t>(target_id);
    }
    data.follow_leader_id = std::numeric_limits<uint32_t>::max();
    const int64_t leader_id = static_cast<int64_t>(dict.get("follow_leader_id", static_cast<int64_t>(-1)));
    if (leader_id >= 0 && leader_id <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
        data.follow_leader_id = static_cast<uint32_t>(leader_id);
    }
    data.has_last_known_target_position = dict.has("last_known_target_x") && dict.has("last_known_target_y");
    data.last_known_target_position = Vector2i(
        static_cast<int>(dict.get("last_known_target_x", 0)),
        static_cast<int>(dict.get("last_known_target_y", 0))
    );
    data.lost_target_turns = std::max(0, static_cast<int>(dict.get("lost_target_turns", 0)));
    data.wander_radius = static_cast<float>(static_cast<double>(dict.get("wander_radius", 4.0)));
    data.wait_turns = std::max(0, static_cast<int>(dict.get("wait_turns", 0)));
    data.forced_reaction = static_cast<bool>(dict.get("forced_reaction", false));
    data.calm_scan_countdown = 0;
    data.blocked_move_count = 0;
    data.path_retry_countdown = 0;
}

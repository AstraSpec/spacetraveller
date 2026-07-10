#include "ai_controller.h"
#include "locomotion.h"
#include "entities/entity.h"
#include "world/world_bubble.h"
#include "data/tile_db.h"
#include "path/path_result.h"
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <utility>
#include <vector>

using namespace godot;

static Vector2i pick_wander_target(
    const AIData& ai,
    const Entity& self,
    const std::function<bool(Vector2i)>& can_enter
) {
    for (int attempt = 0; attempt < 20; attempt++) {
        int ox = UtilityFunctions::randi_range(-static_cast<int>(ai.wander_radius),
                                                static_cast<int>(ai.wander_radius));
        int oy = UtilityFunctions::randi_range(-static_cast<int>(ai.wander_radius),
                                                static_cast<int>(ai.wander_radius));
        int tx = ai.wander_center.x + ox;
        int ty = ai.wander_center.y + oy;
        if (tx == self.x && ty == self.y) continue;

        Vector2i target(tx, ty);
        if (can_enter(target)) return target;
    }
    return Vector2i(self.x, self.y);
}

static bool is_adjacent(const Vector2i& a, const Vector2i& b) {
    int dx = abs(a.x - b.x);
    int dy = abs(a.y - b.y);
    return dx <= 1 && dy <= 1 && (dx + dy) > 0;
}

static int chebyshev_distance(const Vector2i& a, const Vector2i& b) {
    return std::max(abs(a.x - b.x), abs(a.y - b.y));
}

static constexpr int FOLLOW_RADIUS = 2;

static std::vector<Vector2i> follow_candidates(const Vector2i& target) {
    static const Vector2i offsets[] = {
        Vector2i(-2, -2), Vector2i(0, -2), Vector2i(2, -2),
        Vector2i(-2, 0),                         Vector2i(2, 0),
        Vector2i(-2, 2),  Vector2i(0, 2),  Vector2i(2, 2),
        Vector2i(-3, -3), Vector2i(0, -3), Vector2i(3, -3),
        Vector2i(-3, 0),                          Vector2i(3, 0),
        Vector2i(-3, 3),  Vector2i(0, 3),  Vector2i(3, 3)
    };

    std::vector<Vector2i> candidates;
    candidates.reserve(16);
    for (int i = 0; i < 16; ++i) {
        candidates.push_back(target + offsets[i]);
    }
    return candidates;
}

AIState AIController::state_from_string(const String& value) {
    String normalized = value.to_lower();
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
    String normalized = value.to_lower();
    return normalized == "wander" || normalized == "combat" ||
        normalized == "idle" || normalized == "follow" || normalized == "flee" || normalized == "guard";
}

String AIController::normalize_disposition(const String& value) {
    String normalized = value.to_lower();
    if (normalized == "friendly" || normalized == "neutral" || normalized == "hostile" ||
        normalized == "fearful" || normalized == "passive") {
        return normalized;
    }
    return "neutral";
}

EntityRelation AIController::relation_from_string(const String& value) {
    String normalized = value.to_lower();
    if (normalized == "friendly") return EntityRelation::FRIENDLY;
    if (normalized == "hostile") return EntityRelation::HOSTILE;
    return EntityRelation::NEUTRAL;
}

String AIController::relation_to_string(EntityRelation value) {
    switch (value) {
        case EntityRelation::FRIENDLY: return "friendly";
        case EntityRelation::HOSTILE: return "hostile";
        case EntityRelation::NEUTRAL: break;
    }
    return "neutral";
}

Intent AIController::tick(AIData& ai, LocomotionData& loco, const AIContext& ctx) {
    if (ai.state == AIState::IDLE || ai.state == AIState::GUARD) {
        Locomotion::clear_path(loco);
        ai.has_follow_target_position = false;
        return Intent{IntentType::NONE};
    }

    if (ai.state != AIState::WANDER && !ctx.has_target) {
        if (ai.state == AIState::FOLLOW) {
            // A follower should wait for a temporarily unavailable leader
            // rather than silently becoming a wandering NPC.
            Locomotion::clear_path(loco);
            ai.stuck_counter = 0;
            return Intent{IntentType::NONE};
        }
        ai.state = AIState::WANDER;
        ai.target_entity_id = std::numeric_limits<uint32_t>::max();
        Locomotion::clear_path(loco);
    }

    if (ai.state != AIState::FOLLOW) {
        ai.has_follow_target_position = false;
    }

    if (ai.state == AIState::FOLLOW &&
        (!ai.has_follow_target_position || ai.follow_target_position != ctx.target_pos)) {
        ai.follow_target_position = ctx.target_pos;
        ai.has_follow_target_position = true;
        Locomotion::clear_path(loco);
    }

    Vector2i next_tile;
    if (Locomotion::peek_next_step(loco, next_tile)) {
        bool path_stale = false;
        if ((ai.state == AIState::COMBAT || ai.state == AIState::FOLLOW || ai.state == AIState::FLEE) && ctx.has_target) {
            if (ai.state == AIState::COMBAT && ctx.perception.player_seen && loco.path_goal != ctx.target_pos) {
                path_stale = true;
            } else if (ai.state == AIState::FOLLOW &&
                       (!ctx.target_same_level ||
                        (ai.has_follow_target_position && ai.follow_target_position != ctx.target_pos))) {
                path_stale = true;
            }
        }
        const bool follower_is_close = ai.state == AIState::FOLLOW && ctx.target_same_level &&
            chebyshev_distance(Vector2i(ctx.self.x, ctx.self.y), ctx.target_pos) <= FOLLOW_RADIUS;
        if (!path_stale && !follower_is_close && is_adjacent(Vector2i(ctx.self.x, ctx.self.y), next_tile)) {
            return Intent{IntentType::MOVE, next_tile};
        }
        Locomotion::clear_path(loco);
    }

    if (ai.state == AIState::WANDER) {
        if (ai.wander_cooldown > 0) {
            ai.wander_cooldown--;
            return Intent{IntentType::NONE};
        }

        Vector2i target = pick_wander_target(ai, ctx.self, ctx.can_enter);
        if (target.x == ctx.self.x && target.y == ctx.self.y) {
            ai.wander_cooldown = 10;
            return Intent{IntentType::NONE};
        }

        PathResult result = ctx.find_path(Vector2i(ctx.self.x, ctx.self.y), target);
        if (result.found && result.waypoints.size() >= 1) {
            Locomotion::set_path(loco, result.waypoints, target);

            if (Locomotion::peek_next_step(loco, next_tile)) {
                return Intent{IntentType::MOVE, next_tile};
            }
        }

        ai.wander_cooldown = 5 + ai.stuck_counter * 2;
        ai.stuck_counter++;
        return Intent{IntentType::NONE};
    }

    if (ai.state == AIState::FOLLOW) {
        const Vector2i self_pos(ctx.self.x, ctx.self.y);

        if (!ctx.target_same_level) {
            // Vertical traversal is intentionally deferred.  Keep the order
            // active so the NPC resumes as soon as both entities share a z.
            Locomotion::clear_path(loco);
            ai.stuck_counter = 0;
            return Intent{IntentType::NONE};
        }

        if (!ctx.has_target || chebyshev_distance(self_pos, ctx.target_pos) <= FOLLOW_RADIUS) {
            Locomotion::clear_path(loco);
            ai.stuck_counter = 0;
            return Intent{IntentType::NONE};
        }

        // A normal route to the leader is the cheapest and most natural
        // option.  Stop at the first waypoint inside the follow radius,
        // before the path reaches the leader's occupied cell.
        PathResult direct_result = ctx.find_path(self_pos, ctx.target_pos);
        if (direct_result.found && !direct_result.waypoints.empty()) {
            std::vector<Vector2i> approach_path;
            approach_path.reserve(direct_result.waypoints.size());
            for (const Vector2i& waypoint : direct_result.waypoints) {
                approach_path.push_back(waypoint);
                if (chebyshev_distance(waypoint, ctx.target_pos) <= FOLLOW_RADIUS) {
                    break;
                }
            }

            if (!approach_path.empty() && ctx.can_enter(approach_path.back())) {
                const Vector2i approach_goal = approach_path.back();
                Locomotion::set_path(loco, approach_path, approach_goal);
                if (Locomotion::peek_next_step(loco, next_tile)) {
                    return Intent{IntentType::MOVE, next_tile};
                }
            }
        }

        // If the direct route is blocked near the leader, try a small ring of
        // alternative free cells at radius two, then radius three.
        PathResult best_result;
        Vector2i best_goal;
        size_t best_length = std::numeric_limits<size_t>::max();
        for (const Vector2i& candidate : follow_candidates(ctx.target_pos)) {
            if (!ctx.can_enter(candidate)) {
                continue;
            }

            PathResult result = ctx.find_path(self_pos, candidate);
            if (!result.found || result.waypoints.empty() || result.waypoints.size() >= best_length) {
                continue;
            }

            best_length = result.waypoints.size();
            best_goal = candidate;
            best_result = std::move(result);
        }

        if (best_result.found && !best_result.waypoints.empty()) {
            Locomotion::set_path(loco, best_result.waypoints, best_goal);
            if (Locomotion::peek_next_step(loco, next_tile)) {
                return Intent{IntentType::MOVE, next_tile};
            }
        }

        // The next turn will retry all candidate cells.  Keep FOLLOW intact
        // while doors, entities, or a narrow corridor temporarily block us.
        Locomotion::clear_path(loco);
        ai.stuck_counter++;
        return Intent{IntentType::NONE};
    }

    if (ai.state == AIState::FLEE) {
        const int dx = ctx.self.x - ctx.target_pos.x;
        const int dy = ctx.self.y - ctx.target_pos.y;
        const int step_x = dx == 0 ? 0 : (dx > 0 ? 1 : -1);
        const int step_y = dy == 0 ? 0 : (dy > 0 ? 1 : -1);
        const int flee_distance = std::max(3, static_cast<int>(ai.wander_radius));
        const Vector2i flee_target(ctx.self.x + step_x * flee_distance, ctx.self.y + step_y * flee_distance);
        PathResult result = ctx.find_path(Vector2i(ctx.self.x, ctx.self.y), flee_target);
        if (result.found && result.waypoints.size() >= 1) {
            Locomotion::set_path(loco, result.waypoints, flee_target);
            if (Locomotion::peek_next_step(loco, next_tile)) {
                return Intent{IntentType::MOVE, next_tile};
            }
        }
        ai.stuck_counter++;
        return Intent{IntentType::NONE};
    }

    // COMBAT state
    if (ai.perception_tier == PerceptionTier::NONE) {
        return Intent{IntentType::NONE};
    }
    const Vector2i target_pos = ctx.target_pos;

    if (is_adjacent(Vector2i(ctx.self.x, ctx.self.y), target_pos)) {
        ai.stuck_counter = 0;
        return Intent{IntentType::MOVE, target_pos};
    }

    PathResult result = ctx.find_path(Vector2i(ctx.self.x, ctx.self.y), target_pos);
    if (result.found && result.waypoints.size() >= 1) {
        Locomotion::set_path(loco, result.waypoints, target_pos);

        if (Locomotion::peek_next_step(loco, next_tile)) {
            return Intent{IntentType::MOVE, next_tile};
        }
    }

    ai.stuck_counter++;
    return Intent{IntentType::NONE};
}

Dictionary AIController::serialize(const AIData& data) {
    Dictionary d;
    d["state"] = state_to_string(data.state);
    d["disposition"] = normalize_disposition(data.disposition);
    if (data.target_entity_id != std::numeric_limits<uint32_t>::max()) {
        d["target_entity_id"] = static_cast<int64_t>(data.target_entity_id);
    }
    std::vector<uint32_t> relation_targets;
    relation_targets.reserve(data.relations.size());
    for (const auto& pair : data.relations) {
        if (pair.second != EntityRelation::NEUTRAL) relation_targets.push_back(pair.first);
    }
    std::sort(relation_targets.begin(), relation_targets.end());
    Array relations;
    for (uint32_t target_id : relation_targets) {
        Dictionary relation;
        relation["target_id"] = static_cast<int64_t>(target_id);
        relation["relation"] = relation_to_string(data.relations.at(target_id));
        relations.push_back(relation);
    }
    if (!relations.is_empty()) d["relations"] = relations;
    d["perception_tier"] = static_cast<int>(data.perception_tier);
    d["wander_center_x"] = data.wander_center.x;
    d["wander_center_y"] = data.wander_center.y;
    d["wander_radius"] = data.wander_radius;
    d["wander_cooldown"] = data.wander_cooldown;
    d["stuck_counter"] = data.stuck_counter;
    return d;
}

void AIController::deserialize(AIData& data, const Dictionary& dict) {
    data.state = state_from_string(String(dict.get("state", "wander")));
    data.disposition = normalize_disposition(String(dict.get("disposition", "neutral")));
    data.target_entity_id = std::numeric_limits<uint32_t>::max();
    const int64_t target_id = static_cast<int64_t>(dict.get("target_entity_id", static_cast<int64_t>(-1)));
    if (target_id >= 0 && target_id <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
        data.target_entity_id = static_cast<uint32_t>(target_id);
    }
    data.relations.clear();
    Variant relations_var = dict.get("relations", Array());
    if (relations_var.get_type() == Variant::ARRAY) {
        Array relations = relations_var;
        for (int i = 0; i < relations.size(); i++) {
            if (relations[i].get_type() != Variant::DICTIONARY) continue;
            Dictionary relation = relations[i];
            const int64_t relation_target = static_cast<int64_t>(relation.get("target_id", static_cast<int64_t>(-1)));
            if (relation_target < 0 || relation_target > static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) continue;
            const EntityRelation value = relation_from_string(String(relation.get("relation", "neutral")));
            if (value != EntityRelation::NEUTRAL) {
                data.relations[static_cast<uint32_t>(relation_target)] = value;
            }
        }
    }
    data.perception_tier = static_cast<PerceptionTier>(static_cast<int>(dict.get("perception_tier", 0)));
    data.wander_center = Vector2i(
        static_cast<int>(dict.get("wander_center_x", 0)),
        static_cast<int>(dict.get("wander_center_y", 0))
    );
    data.wander_radius = static_cast<float>(static_cast<double>(dict.get("wander_radius", 10.0)));
    data.wander_cooldown = static_cast<int>(dict.get("wander_cooldown", 0));
    data.stuck_counter = static_cast<int>(dict.get("stuck_counter", 0));
}
